#ifndef MODBUS_CORE_H
#define MODBUS_CORE_H

#include <stdint.h>
#include "modbus_config.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum {
    MODBUS_SERVICE_IDLE 		= 0,
    MODBUS_SERVICE_PROCESSING,
    MODBUS_SERVICE_RESPONSE_QUEUED,
    MODBUS_SERVICE_ERROR
} ModBusServiceStatus;

typedef enum {
    MODBUS_ACCESS_OK 			= 0,
    MODBUS_ACCESS_INVALID_ADDRESS,
    MODBUS_ACCESS_DENIED,
    MODBUS_ACCESS_FATAL
} ModBusAccessStatus;

void ModBusInit(
    ModBusPersistentConfig *persistent,
    ModBusRuntimeConfig *runtime
);

ModBusServiceStatus ModBusService(void);

void ModBusNotifyTick(void);

#ifdef __cplusplus
	}
#endif

#endif /* MODBUS_CORE_H */
