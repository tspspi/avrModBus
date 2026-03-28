#include "modbus_config.h"
#include <string.h>

/*
	Default UUIDs

	These UUIDs are applied by default if not overriden by the application.
*/
static const uint16_t DefaultDeviceTypeUuid[8] = {
    0x5bd7, 0x41d2, 0x9bb0, 0x4726,
    0x8fc2, 0x54f4, 0xaa7f, 0xcc24
};

static const uint16_t DefaultDeviceInstanceUuid[8] = {
    0xf191, 0x5fb7, 0xcfd8, 0x43aa,
    0xa798, 0x123d, 0x9658, 0x010b
};

void ModBusLoadDefaultPersistent(ModBusPersistentConfig *cfg) {
    if (cfg == NULL) {
        return;
    }

    cfg->DeviceId = MODBUS_DEFAULT_DEVICE_ID;
    cfg->BaudRateEnum = MODBUS_DEFAULT_BAUD_ENUM;
    cfg->ResetCounter = 0u;
}

void ModBusLoadDefaultRuntime(ModBusRuntimeConfig *cfg) {
    if (cfg == NULL) {
        return;
    }

    memcpy(cfg->DeviceTypeUuid, DefaultDeviceTypeUuid, sizeof(DefaultDeviceTypeUuid));
    memcpy(cfg->DeviceInstanceUuid, DefaultDeviceInstanceUuid, sizeof(DefaultDeviceInstanceUuid));

    cfg->HoldingBase = 0u;
    cfg->HoldingLength = 0u;
    cfg->InputBase = 0u;
    cfg->InputLength = 0u;
    cfg->CoilBase = 0u;
    cfg->CoilBitLength = 0u;

    cfg->Flags = 0u;
    cfg->Reserved = 0u;

    cfg->HoldingData = NULL;
    cfg->InputData = NULL;
    cfg->CoilBitmap = NULL;
}
