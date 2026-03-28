# AVR ModBus Library – User Guide

## Prerequisites
- avr-gcc toolchain (avr-gcc, avr-ar, avr-objcopy, avr-objdump) accessible via the fixed prefix `avr-`.
- GNU make (`gmake`) on the host (FreeBSD default `make` is insufficient).
- Hardware: ATMega328P (UART0) or ATMega2560 (UART1+) with an RS485 transceiver (MAX485) connected to the selected UART and a controllable DE/RE pin.

## Building the Library
```sh
gmake MCU=atmega328p F_CPU=16000000UL
```
Outputs `libavrmodbus.a` alongside object files in `build/`. Adjust `MCU`/`F_CPU` as needed for the target device.

## Integrating in Your Firmware
1. **Configuration structures**
   - Allocate `ModBusPersistentConfig` in RAM, load it from EEPROM (your application stores version + CRC). Pass the pointer to `ModBusInit()`.
   - Allocate/fill `ModBusRuntimeConfig` with UUID words, base offsets, lengths, and pointers to register/coil storage (or leave null to rely purely on callbacks).
2. **Initialization**
   ```c
   ModBusPersistentConfig persistent;
   ModBusRuntimeConfig runtime;
   ModBusLoadDefaultPersistent(&persistent);
   ModBusLoadDefaultRuntime(&runtime);
   // load EEPROM into persistent + custom runtime fields
   ModBusInit(&persistent, &runtime);
   ```
3. **Main loop**
   - Call `ModBusService()` frequently (≈ every character period or faster).
   - Call `ModBusNotifyTick()` at a fixed cadence (default assumptions ≈ 1 kHz) so the parser detects 1.5/3.5 character gaps. Either:
     - Have your scheduler invoke it (e.g., from a timer ISR you own), or
     - Enable the built-in timer hook via `#define MODBUS_ENABLE_INTERNAL_TICK 1` and choose Timer0/1 along with prescalers/frequency macros.
4. **Callbacks vs. memory mapping**
   - Provide arrays for contiguous holding/input registers and coil bitmaps via `ModBusRuntimeConfig`. The library will read/write directly when addresses land in these ranges.
   - Override weak callbacks (`modbusHandleHoldingWrite`, etc.) to handle sparse/custom registers or to validate writes. Return the appropriate `ModBusAccessStatus` so the frame handler answers with standard ModBus exceptions.
5. **RS485 DE pin**
   - Set `MODBUS_MAX485_DE_*` macros in `modbus_config.h` (via compiler `-D` flags or edits) to match your board if PORTD/PD2 isn’t correct.
6. **Config changes**
   - Writes to holding register `0` change the device ID (valid range 1–247).
   - Register `1` changes baud enumeration (0=2400 … 6=115200 double-speed). They take effect after the application persists config and reinitializes.
   - Writing `0xAA55` to holding register `255` increments `ResetCounter` then calls `modbusConfigChangeResetRequest()`. Override this weak function to store the persistent struct to EEPROM and trigger a restart (e.g., watchdog).

## Optional Timer Hook
- Disabled by default (`MODBUS_ENABLE_INTERNAL_TICK 0`). When set to `1`, the library configures Timer0 or Timer1 (select via `MODBUS_TICK_TIMER`) in CTC mode to invoke `ModBusNotifyTick()` automatically.
- Additional macros control prescalers and tick frequency:
  - `MODBUS_TICK_TIMER0_PRESCALER`, `MODBUS_TICK_TIMER1_PRESCALER`
  - `MODBUS_TICK_FREQUENCY_HZ` (default 1000 Hz)
- Ensure these resources are free; the library will reconfigure the timer registers when enabled.

## Example Overview
The `examples/basic` target demonstrates:
- Loading/storing `ModBusPersistentConfig` within an application-defined EEPROM blob (with version + CRC).
- Toggling an LED via a user holding register or ModBus coil write.
- Reporting a switch state and an ADC channel reading through input registers.
- Using the optional timer hook (Timer0 CTC) to drive ticks automatically.

Build it via `gmake example`. Flashing is left to the example’s Makefile (`gmake flash`) and requires avrdude; adjust programmer/port there.

## Troubleshooting
- **No response on bus:** Confirm `ModBusService()` and `ModBusNotifyTick()` run frequently enough; check DE pin macros and that interrupts are enabled.
- **CRC errors:** Ensure the UART baud enum matches actual wiring and that F_CPU is correct at compile time.
- **Config writes not persistent:** Verify the application overrides `modbusConfigChangeResetRequest()` and commits the RAM struct to EEPROM before resetting.
- **Build errors on FreeBSD:** Always invoke `gmake` and install the AVR toolchain; BSD `make` and host `cc` will fail due to unsupported flags.
