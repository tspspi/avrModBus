#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef MODBUS_UART_RX_BUFFER_SIZE
	#define MODBUS_UART_RX_BUFFER_SIZE 		128u
#endif

#ifndef MODBUS_UART_TX_BUFFER_SIZE
	#define MODBUS_UART_TX_BUFFER_SIZE		128u
#endif

#define MODBUS_DEFAULT_DEVICE_ID        		1u
#define MODBUS_DEFAULT_BAUD_ENUM        		2u

#ifndef MODBUS_INTER_BYTE_TIMEOUT_TICKS
	#define MODBUS_INTER_BYTE_TIMEOUT_TICKS 	2u
#endif

#ifndef MODBUS_FRAME_GAP_TICKS
	#define MODBUS_FRAME_GAP_TICKS 		 	4u
#endif

#define MODBUS_REG_HOLDING_DEVICE_ID   			0u
#define MODBUS_REG_HOLDING_BAUD_ENUM   			1u
#define MODBUS_REG_HOLDING_RESET_MAGIC 			255u

#define MODBUS_INPUT_UUID_DEVICE_BASE   		0u
#define MODBUS_INPUT_UUID_INSTANCE_BASE 		8u
#define MODBUS_UUID_WORDS 				8u

#ifndef MODBUS_ENABLE_INTERNAL_TICK
	#define MODBUS_ENABLE_INTERNAL_TICK 		1u
#endif

#ifndef MODBUS_TICK_TIMER
	#define MODBUS_TICK_TIMER 			0u /* 0 = Timer0, 1 = Timer1 */
#endif

#ifndef MODBUS_TICK_FREQUENCY_HZ
	#define MODBUS_TICK_FREQUENCY_HZ 		1000u
#endif

#ifndef MODBUS_TICK_TIMER0_PRESCALER
	#define MODBUS_TICK_TIMER0_PRESCALER 		64u
#endif

#ifndef MODBUS_TICK_TIMER1_PRESCALER
	#define MODBUS_TICK_TIMER1_PRESCALER		64u
#endif

#define MODBUS_RUNTIME_FLAG_SYSCLOCK    	(1u << 0)
#define MODBUS_RUNTIME_FLAG_CALLBACKS   	(1u << 1)

typedef struct {
    uint8_t DeviceId;
    uint8_t BaudRateEnum;
    uint16_t ResetCounter;
} ModBusPersistentConfig;

typedef struct {
    uint16_t DeviceTypeUuid[8];
    uint16_t DeviceInstanceUuid[8];

    uint16_t HoldingBase;
    uint16_t HoldingLength;
    uint16_t InputBase;
    uint16_t InputLength;
    uint16_t CoilBase;
    uint16_t CoilBitLength;

    uint16_t Flags;
    uint16_t Reserved;

    uint16_t *HoldingData;
    const uint16_t *InputData;
    uint8_t *CoilBitmap;
} ModBusRuntimeConfig;

void ModBusLoadDefaultPersistent(
    ModBusPersistentConfig *cfg
);

void ModBusLoadDefaultRuntime(
    ModBusRuntimeConfig *cfg
);

#ifdef __cplusplus
	}
#endif

#endif /* MODBUS_CONFIG_H */
