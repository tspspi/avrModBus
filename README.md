# avrModBus - ModBus RTU slave toolkit for AVR

> _Disclaimer_: This project has partially been coded in cooperation with
> an AI assistant. All design decisions have been made by a human, all code
> has been manually reviewed.

`avrModBus` is a small ANSI C library targeting `ATMega328P` and `ATMega2560`
that turns an AVR into a ModBus RTU slave over RS485 (for example with an MAX485).
It allows to use an internal system clock module for easy and quick implementations
or providing a custom system clock module. The host application integrates ModBus servicing
into its own control loop without rewriting UART or timeout logic.

Key features are:

- ModBus function handlers (0x01–0x10)
- Callback and memory-backed register handling for holding/input/coil blocks
- Persistent configuration hand-off (`ModBusPersistentConfig`) and runtime descriptors
- Optional tick timer module that auto calls `ModBusNotifyTick()` when not using a custom
  sysclock module.
- Basic example firmware demonstrating EEPROM managed config, LED/coil control, switch
  and ADC reporting and watchdog based resets

The library avoids external dependencies beyond the `avr-gcc` toolchain and can be
linked as `libavrmodbus.a` inside firmware.

## Repository layout

```
.
├── include/           # Public headers (config, core, registers, hardware)
├── src/               # Library sources (config helpers, core, hw, weak callbacks)
├── examples/basic/    # Mega/328P demo app with Makefile + flash target
├── docs/              # Design notes, user guide, TODOs
└── build/             # Generated objects (ignored)
```

## Building the static library

```bash
gmake MCU=atmega2560 F_CPU=16000000UL MODBUS_DEFS="-DMODBUS_UART_INSTANCE=1"
```

Outputs `libavrmodbus.a` under the repository root and object files in `build/`. Override
`MCU`, `F_CPU`, and `MODBUS_DEFS` (e.g., to select UART0) as needed.

## Example firmware

The `examples/basic` directory contains a self contained demo that:

- Loads a persistent configuration blob from EEPROM
- Maps holding registers 16–19 to RAM, input registers 32–35 to switch/ADC data
- Provides coil 0 for LED control and watches for writes to holding register 255 to
  trigger `modbusConfigChangeResetRequest()`
- Uses the optional tick timer (Timer0 CTC @ 1 kHz) to drive `ModBusNotifyTick()`

Build and flash (Arduino Mega 2560 wiring bootloader) via:

```bash
gmake example MCU=atmega2560 MODBUS_DEFS='-DMODBUS_UART_INSTANCE=1'
gmake -C examples/basic flash MCU=atmega2560 FLASHDEV=/dev/ttyU1 FLASHMETHOD=wiring FLASHBAUD=115200 MODBUS_DEFS='-DMODBUS_UART_INSTANCE=1'
```

## Configuration overview

`include/modbus_config.h` exposes all compile-time switches:

- `MODBUS_UART_INSTANCE` and MAX485 pin macros select the UART/DE pin mapping
- RX/TX buffer sizes are set via `MODBUS_UART_RX_BUFFER_SIZE` and `MODBUS_UART_TX_BUFFER_SIZE`
- Timing constants (`MODBUS_INTER_BYTE_TIMEOUT_TICKS`, `MODBUS_FRAME_GAP_TICKS`)
- Optional tick timer (`MODBUS_ENABLE_INTERNAL_TICK`, `MODBUS_TICK_TIMER*` macros)
- Default IDs, UUIDs, runtime flags, and helper loaders for persistent/runtime structs

Applications instantiate `ModBusPersistentConfig` from EEPROM and fill a
`ModBusRuntimeConfig` with pointers to holding/input data or coil bitmaps. Weak callbacks in
`modbus_registers.c` can be overridden for sparse/custom behavior.

## Using the library

1. Initialize config structs:
   ```c
   ModBusPersistentConfig persistent;
   ModBusRuntimeConfig runtime;
   ModBusLoadDefaultPersistent(&persistent);
   ModBusLoadDefaultRuntime(&runtime);
   // customize runtime.HoldingBase/InputBase/etc.
   ModBusInit(&persistent, &runtime);
   ```
2. Call `ModBusService()` frequently (or at least often enough to drain the RX ring). Only
   byte queuing happens in ISRs; frame assembly, CRC checks, and function handling occur inside
   `ModBusService()`. The AVR never responds without calling `ModBusService()`.
3. Provide `ModBusNotifyTick()` pulses at around 1 kHz so the parser can detect the 1.5/3.5 character
   gaps. Either enable the internal timer hook or call it from your scheduler.
4. Override weak callbacks for EEPROM persistence (`modbusConfigChangeResetRequest`) and any
   custom register logic you need.
