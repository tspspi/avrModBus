#ifndef MODBUS_HW_H
#define MODBUS_HW_H

#include <stdint.h>
#include <avr/io.h>

#include "modbus_config.h"

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef MODBUS_UART_INSTANCE
	#define MODBUS_UART_INSTANCE		1u
#endif

#ifndef MODBUS_MAX485_DE_PORT
	#define MODBUS_MAX485_DE_PORT		PORTA
#endif

#ifndef MODBUS_MAX485_DE_DDR
	#define MODBUS_MAX485_DE_DDR 		DDRA
#endif

#ifndef MODBUS_MAX485_DE_PIN
	#define MODBUS_MAX485_DE_PIN 		PA0
#endif

#ifndef MODBUS_MAX485_DE_ACTIVE_HIGH
	#define MODBUS_MAX485_DE_ACTIVE_HIGH	1
#endif

typedef struct {
    volatile uint8_t Head;
    volatile uint8_t Tail;
    volatile uint8_t Count;
    uint8_t Size;
    volatile uint8_t *Buffer;
} ModBusRingBuffer;

void ModBusHwInit(
    ModBusPersistentConfig *persistent,
    const ModBusRuntimeConfig *runtime
);

void ModBusHwEnableTransmitter(void);
void ModBusHwDisableTransmitter(void);

uint8_t ModBusHwRxAvailable(void);
uint8_t ModBusHwTxSpace(void);

int ModBusHwReadByte(uint8_t *byte);
int ModBusHwWriteByte(uint8_t byte);

void ModBusHwFlushTransmit(void);

#ifdef __cplusplus
	}
#endif

#endif /* MODBUS_HW_H */
