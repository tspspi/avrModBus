# TODO

- [x] Define `struct ModBusPersistentConfig` (DeviceId, BaudRateEnum, ResetCounter) and `struct ModBusRuntimeConfig` (UUIDs, register pointers/lengths, coil bitmap, flags) plus init helpers and documentation on EEPROM interaction.
- [x] Specify API for memory-mapped register blocks and weak callbacks, including error codes for invalid accesses.
- [x] Draft header scaffolding (`modbus_config.h`, `modbus_core.h`, `modbus_registers.h`) reflecting callbacks, buffer sizes, UUID overrides, and config struct hand-off.
- [x] Implement UART/RS485 hardware layer with RX/TX ring buffers and weak DE/RE control hooks.
- [x] Implement `ModBusService()` core parser, CRC16, timeout handling (1.5/3.5 char gaps), and response pipeline.
- [-] Add sysclock optional module and determine integration when disabled.
- [x] Add optional compile-time timer hook (Timer0/Timer1) that can auto-call ModBusNotifyTick() when enabled; default off.
- [x] Create example application under `examples/basic/` with Makefile (including non-default `flash` target) demonstrating register access and LED/switch interactions.
- [x] Document user integration steps and configuration options in user-facing docs (to be created).
- [-] Populate KB references when external research becomes necessary (update `KB/index.md`).

- [x] Verify ModBus reads of holding registers 0-1 (device ID, baud enum).
- [x] Verify single and multiple coil writes toggle LED and read back correctly (Write Single tested twice with 2 s gap).
- [x] Test holding-register changes (ID/baud), trigger reset register, and confirm new ID works, then revert.
- [x] Validate analog input register reporting (ADC channel sample).
- [x] Validate digital input register reporting (switch).
- [x] Validate UUID input registers (device type & instance).
- [ ] Check behavior on a multi-device bus (collisions/response timing).
