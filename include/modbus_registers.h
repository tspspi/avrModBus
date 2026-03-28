#ifndef MODBUS_REGISTERS_H
#define MODBUS_REGISTERS_H

#include <stdint.h>
#include "modbus_core.h"

#ifdef __cplusplus
	extern "C" {
#endif

ModBusAccessStatus modbusHandleHoldingRead(
	uint16_t address,
	uint16_t *value
);
ModBusAccessStatus modbusHandleHoldingWrite(
	uint16_t address,
	uint16_t value
);

ModBusAccessStatus modbusHandleInputRead(
	uint16_t address,
	uint16_t *value
);

ModBusAccessStatus modbusHandleCoilRead(
	uint16_t address,
	uint8_t *value
);
ModBusAccessStatus modbusHandleCoilWrite(
	uint16_t address,
	uint8_t value
);

void modbusConfigChangeResetRequest(void);

#ifdef __cplusplus
	}
#endif

#endif /* MODBUS_REGISTERS_H */
