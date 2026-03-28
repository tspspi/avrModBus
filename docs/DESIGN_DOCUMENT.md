# AVR ModBus Client Library Design

## Goals
- Provide a reusable ANSI C (avr-gcc) library to implement ModBus RTU slaves on ATMega328P (UART0) and ATMega2560 (UART1) over RS485 (MAX485 transceivers).
- Expose predictable APIs so applications can integrate ModBus handling without disturbing time-critical control loops.
- Ship an example in `examples/` demonstrating configuration registers and simple I/O (LED + switch) along with a Makefile (optional `flash` target).

## Key Requirements
1. **Hardware targets**: ATMega328P UART0, ATMega2560 UART1 with identical source base.
2. **Serial link defaults**: 9600 baud, 8 data bits, no parity, 1 stop bit; only baud and slave address are runtime configurable (effective after restart).
3. **RS485 half-duplex**: Manage MAX485 DE/RE pins so TX occurs only when sending frames. Pin selections and polarity are compile-time constants defined in `modbus_config.h` to avoid runtime overhead.
4. **Interrupt architecture**:
   - UART RX ISR pushes bytes into a compile-time configurable ring buffer (default 128 bytes).
   - UART TX ISR pulls bytes from a TX buffer to keep bus utilization predictable.
   - All higher-level parsing happens outside ISRs.
5. **Processing model**:
   - Application calls `ModBusService()` frequently (main loop or scheduled task) to pull bytes from RX buffer, assemble frames, enforce timing, process requests, and queue replies.
   - Application also triggers `ModBusNotifyTick()` at a fixed cadence (≈1 tick per ModBus character time by default) so the core can detect the 1.5/3.5 character gaps; an optional timer module will be able to call it automatically when enabled.
   - Optional weak-linked callbacks notify application about events (full frame received, coil/holding writes, input register reads, etc.). Default weak implementations do nothing to avoid linker errors.
   - A `modbusConfigChangeResetRequest()` weak callback is raised when configuration must be persisted and a restart performed; example apps override it to save EEPROM and trigger their preferred reboot path.
6. **Timer usage**:
   - Library should avoid seizing Timer0; optionally provide a sysclock module (millis, micros) behind a compile-time flag. If disabled, timeouts depend on `ModBusService()` being called often enough; we may request host application to provide periodic ticks if necessary.
   - A separate compile-time option (off by default) lets the library claim Timer0 or Timer1 to auto-fire `ModBusNotifyTick()` at a configurable frequency so very simple applications can rely on an ISR-driven tick without writing their own scheduler.
7. **Timeouts/gaps**: Target ModBus RTU gaps (≥1.5 char times inter-byte when receiving; ≥3.5 char times between frames). Implementation must tolerate longer delays if main loop is busy.
8. **Persistent configuration (EEPROM-backed)**:
   - Application defines and owns `struct ModBusPersistentConfig`, stores it in EEPROM, and loads it into RAM before calling `ModBusInit(&persistent, &runtime)`.
   - Fields stored in EEPROM are limited to long-lived parameters: `DeviceId`, `BaudRateEnum`, and `ResetCounter`. The example application additionally tracks its own `ConfigVersion` and CRC covering this struct when committing to EEPROM; the library stays agnostic.
   - Library keeps only a volatile pointer to the structure; on `0xAA55` writes it updates the struct in RAM and asks the host (via `modbusConfigChangeResetRequest()`) to persist to EEPROM and orchestrate the restart.
9. **Runtime descriptors (RAM-only)**:
   - Separate `struct ModBusRuntimeConfig` manages items not written to EEPROM: device-type/instance UUID arrays, application register mappings, coil bitmaps, option flags, etc.
   - Applications pass pointers to their register storage: `const uint16_t *HoldingData`, `const uint16_t *InputData`, and `uint8_t *CoilBitmap`, each accompanied by a base address and element/bit length. The coil bitmap uses packed bits (LSB = lowest coil address within the block).
   - Applications may also rely purely on callbacks by leaving pointer/length pairs zeroed.
10. **Register map**:
   - Holding Register 0: device ID (default 1).
   - Holding Register 1: baud-rate enum (0=2400, 1=4800, 2=9600, ...). Default 2.
   - Holding Register 255: write magic `0xAA55` to apply config and restart.
   - Input Registers 0–7: device-type UUID (default `5bd741d2-9bb0-4726-8fc2-54f4aa7fcc24`).
   - Input Registers 8–15: device-instance UUID (default `f1915fb7-cfd8-43aa-a798-123d9658010b`).
   - Applications can override UUIDs via compile-time definitions or by filling the runtime config struct.
11. **Application extensibility**:
    - Memory-backed register blocks supplied through the runtime config are exposed immediately after the built-in registers using the advertised base addresses.
    - Weak-linked handler functions (e.g., `modbusHandleHoldingWrite`, `modbusHandleInputRead`, `modbusHandleCoilWrite`) provide a callback path. They return a status code so the core can emit ModBus exceptions on invalid access or internal errors.
    - Runtime struct and callbacks can coexist: the core first checks whether the access hits a memory-backed block, otherwise it delegates to the weak handler.
12. **Reset/apply behavior**:
    - When holding register 255 receives `0xAA55`, the library updates the RAM config struct and invokes `modbusConfigChangeResetRequest()` so the application can commit to EEPROM and orchestrate the restart.
13. **Example app**:
    - Demonstrates reading base registers, toggling LED output via holding register/coil, reading a switch via input register, sampling an ADC channel into another input register, and using the optional `flash` make target.
    - Manages its own EEPROM blob (`struct ExamplePersistentStore`) containing a version byte, CRC, and embedded `ModBusPersistentConfig` payload.

## Modules (initial thoughts)
- `modbus_hw.c/.h`: UART/RS485 pin setup, buffer management, ISR definitions.
- `modbus_core.c/.h`: Frame parser, CRC16, register dispatch, timeout state machine, `ModBusService()` implementation.
- `modbus_registers.c/.h`: Default register handlers, weak callbacks, UUID helpers.
- `modbus_config.h`: Compile-time options (ring buffer size, UART selection, sysclock enable, default IDs/UUIDs, MAX485 pin mapping) plus persistent/runtime struct declarations.
- `examples/basic/`: Application demonstrating configuration and I/O.

## Outstanding Decisions
- How `ModBusService()` reports status to caller (e.g., enum for idle/busy/error) for diagnostics.
- Strategy for enforcing the 1.5/3.5 char gaps if app delays `ModBusService()`; may require timer capture or tracking via cycle counter when sysclock module enabled.

## Next Steps
- Define `struct ModBusPersistentConfig` and `struct ModBusRuntimeConfig` in headers with helper routines to initialize defaults.
- Specify EEPROM usage for device ID/baud persistence and add helper to write when magic value detected.
- Outline example application structure and Makefile skeleton.
