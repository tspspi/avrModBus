#include "modbus_registers.h"
#include <stddef.h>

__attribute__((weak)) ModBusAccessStatus modbusHandleHoldingRead(
    uint16_t address,
    uint16_t *value
) {
    (void)address;
    if (value != NULL) {
        *value = 0u;
    }
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

__attribute__((weak)) ModBusAccessStatus modbusHandleHoldingWrite(
    uint16_t address,
    uint16_t value
) {
    (void)address;
    (void)value;
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

__attribute__((weak)) ModBusAccessStatus modbusHandleInputRead(
    uint16_t address,
    uint16_t *value
) {
    (void)address;
    if (value != NULL) {
        *value = 0u;
    }
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

__attribute__((weak)) ModBusAccessStatus modbusHandleCoilRead(
    uint16_t address,
    uint8_t *value
) {
    (void)address;
    if (value != NULL) {
        *value = 0u;
    }
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

__attribute__((weak)) ModBusAccessStatus modbusHandleCoilWrite(
   uint16_t address,
   uint8_t value
) {
    (void)address;
    (void)value;
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

__attribute__((weak)) void modbusConfigChangeResetRequest(void) {
    /* Application provided override should persist config to EEPROM and trigger restart. */
}
