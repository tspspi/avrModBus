#include "modbus_core.h"

#include <stddef.h>
#include <string.h>

#include "modbus_hw.h"
#include "modbus_registers.h"

#define MODBUS_MAX_FRAME_SIZE 			252u

#define MODBUS_FUNC_READ_COILS          	0x01u
#define MODBUS_FUNC_READ_DISCRETE       	0x02u /* reserved, not implemented */
#define MODBUS_FUNC_READ_HOLDING        	0x03u
#define MODBUS_FUNC_READ_INPUT          	0x04u
#define MODBUS_FUNC_WRITE_SINGLE_COIL   	0x05u
#define MODBUS_FUNC_WRITE_SINGLE_REG    	0x06u
#define MODBUS_FUNC_WRITE_MULTIPLE_COIL 	0x0Fu
#define MODBUS_FUNC_WRITE_MULTIPLE_REG  	0x10u

#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 	0x01u
#define MODBUS_EXCEPTION_ILLEGAL_ADDRESS  	0x02u
#define MODBUS_EXCEPTION_ILLEGAL_VALUE    	0x03u
#define MODBUS_EXCEPTION_SERVER_FAILURE   	0x04u

#define MODBUS_MAX_HOLDING_PER_REQUEST 		125u
#define MODBUS_MAX_INPUT_PER_REQUEST   		125u
#define MODBUS_MAX_COILS_PER_REQUEST   		2000u

typedef enum {
    MODBUS_RX_STATE_IDLE = 0,
    MODBUS_RX_STATE_ACTIVE
} ModBusRxState;

typedef struct {
    ModBusRxState State;
    uint8_t Length;
    uint8_t Buffer[MODBUS_MAX_FRAME_SIZE];
    uint16_t SilentTicks;
} ModBusParserContext;

static ModBusPersistentConfig DefaultPersistent;
static ModBusRuntimeConfig DefaultRuntime;

static ModBusPersistentConfig *PersistentConfigPtr = NULL;
static ModBusRuntimeConfig *RuntimeConfigPtr = NULL;
static ModBusPersistentConfig PendingConfig;
static uint8_t ActiveDeviceId = 1u;
static ModBusParserContext Parser;

static uint8_t ResponseBuffer[MODBUS_MAX_FRAME_SIZE];

static void ModBusParserReset(void);
static uint16_t ModBusCrc16(const uint8_t *data, uint8_t length);
static uint8_t ModBusCrcValid(const uint8_t *frame, uint8_t length);
static ModBusServiceStatus ModBusHandleFrame(uint8_t length);
static void ModBusSendResponse(uint8_t *buffer, uint8_t length);
static void ModBusSendException(uint8_t address, uint8_t function, uint8_t code);
static uint16_t ModBusReadU16(const uint8_t *bytes);
static void ModBusWriteU16(uint8_t *dest, uint16_t value);
static uint8_t ModBusIsBroadcast(uint8_t address);
static uint8_t ModBusAccessStatusToException(ModBusAccessStatus status);
static ModBusAccessStatus ModBusHoldingRead(uint16_t address, uint16_t *value);
static ModBusAccessStatus ModBusHoldingWrite(uint16_t address, uint16_t value);
static ModBusAccessStatus ModBusInputReadRegister(uint16_t address, uint16_t *value);
static ModBusAccessStatus ModBusCoilReadBitInternal(uint16_t address, uint8_t *value);
static ModBusAccessStatus ModBusCoilWriteBitInternal(uint16_t address, uint8_t value);
static ModBusServiceStatus ModBusHandleReadHolding(uint8_t slave, uint16_t start, uint16_t quantity);
static ModBusServiceStatus ModBusHandleReadInput(uint8_t slave, uint16_t start, uint16_t quantity);
static ModBusServiceStatus ModBusHandleWriteSingleHolding(uint8_t slave, uint16_t address, uint16_t value, uint8_t broadcast);
static ModBusServiceStatus ModBusHandleWriteMultipleHolding(uint8_t slave, const uint8_t *frame, uint8_t length, uint8_t broadcast);
static ModBusServiceStatus ModBusHandleReadCoils(uint8_t slave, uint16_t start, uint16_t quantity);
static ModBusServiceStatus ModBusHandleWriteSingleCoil(uint8_t slave, uint16_t address, uint16_t value, uint8_t broadcast);
static ModBusServiceStatus ModBusHandleWriteMultipleCoils(uint8_t slave, const uint8_t *frame, uint8_t length, uint8_t broadcast);

void ModBusInit(
    ModBusPersistentConfig *persistent,
    ModBusRuntimeConfig *runtime
) {
    if (persistent == NULL) {
        ModBusLoadDefaultPersistent(&DefaultPersistent);
        PersistentConfigPtr = &DefaultPersistent;
    } else {
        PersistentConfigPtr = persistent;
    }

    if (runtime == NULL) {
        ModBusLoadDefaultRuntime(&DefaultRuntime);
        RuntimeConfigPtr = &DefaultRuntime;
    } else {
        RuntimeConfigPtr = runtime;
    }

    PendingConfig = *PersistentConfigPtr;
    ActiveDeviceId = PersistentConfigPtr->DeviceId;

    ModBusParserReset();
    ModBusHwInit(PersistentConfigPtr, RuntimeConfigPtr);
}

ModBusServiceStatus ModBusService(void) {
    uint8_t byte;
    ModBusServiceStatus status = MODBUS_SERVICE_IDLE;

    while (ModBusHwReadByte(&byte)) {
        if ((Parser.State == MODBUS_RX_STATE_ACTIVE) &&
            (Parser.SilentTicks >= MODBUS_FRAME_GAP_TICKS)) {
            /*
                Frame gap elapsed and no processing started yet – drop the
                buffered bytes so this new frame can be captured cleanly.
            */
            ModBusParserReset();
        }

        if (Parser.Length < MODBUS_MAX_FRAME_SIZE) {
            Parser.Buffer[Parser.Length] = byte;
            Parser.Length = (uint8_t)(Parser.Length + 1u);
            Parser.State = MODBUS_RX_STATE_ACTIVE;
            Parser.SilentTicks = 0u;
            status = MODBUS_SERVICE_PROCESSING;
        } else {
            ModBusParserReset();
            status = MODBUS_SERVICE_ERROR;
            break;
        }
    }

    if ((Parser.State == MODBUS_RX_STATE_ACTIVE) &&
        (Parser.SilentTicks >= MODBUS_FRAME_GAP_TICKS)) {
        status = ModBusHandleFrame(Parser.Length);
        ModBusParserReset();
    }

    return status;
}

void ModBusNotifyTick(void) {
    if (Parser.SilentTicks < 0xFFFFu) {
        Parser.SilentTicks = (uint16_t)(Parser.SilentTicks + 1u);
    }
}

static void ModBusParserReset(void) {
    Parser.State = MODBUS_RX_STATE_IDLE;
    Parser.Length = 0u;
    Parser.SilentTicks = 0u;
    memset(Parser.Buffer, 0, sizeof(Parser.Buffer));
}

static uint16_t ModBusCrc16(const uint8_t *data, uint8_t length) {
    uint16_t crc = 0xFFFFu;

    for (uint8_t pos = 0u; pos < length; pos = (uint8_t)(pos + 1u)) {
        crc ^= data[pos];
        for (uint8_t i = 0u; i < 8u; i = (uint8_t)(i + 1u)) {
            if ((crc & 0x0001u) != 0u) {
                crc >>= 1;
                crc ^= 0xA001u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint8_t ModBusCrcValid(const uint8_t *frame, uint8_t length) {
    if (length < 4u) {
        return 0u;
    }

    uint16_t received = (uint16_t)frame[length - 2u] | ((uint16_t)frame[length - 1u] << 8);
    uint16_t calculated = ModBusCrc16(frame, (uint8_t)(length - 2u));
    return (received == calculated) ? 1u : 0u;
}

static ModBusServiceStatus ModBusHandleFrame(uint8_t length) {
    if (!ModBusCrcValid(Parser.Buffer, length)) {
        return MODBUS_SERVICE_ERROR;
    }

    if (length < 8u) {
        if (Parser.Buffer[0] != 0u) {
            ModBusSendException(Parser.Buffer[0], Parser.Buffer[1], MODBUS_EXCEPTION_ILLEGAL_VALUE);
            return MODBUS_SERVICE_ERROR;
        }
        return MODBUS_SERVICE_IDLE;
    }

    uint8_t slave = Parser.Buffer[0];
    uint8_t function = Parser.Buffer[1];
    uint8_t broadcast = ModBusIsBroadcast(slave);

    if ((!broadcast) && (slave != ActiveDeviceId)) {
        return MODBUS_SERVICE_IDLE;
    }

    uint16_t startAddress;
    uint16_t quantity;

    switch (function) {
        case MODBUS_FUNC_READ_HOLDING:
            if (broadcast) {
                return MODBUS_SERVICE_IDLE;
            }
            startAddress = ModBusReadU16(&Parser.Buffer[2]);
            quantity = ModBusReadU16(&Parser.Buffer[4]);
            return ModBusHandleReadHolding(slave, startAddress, quantity);

        case MODBUS_FUNC_READ_INPUT:
            if (broadcast) {
                return MODBUS_SERVICE_IDLE;
            }
            startAddress = ModBusReadU16(&Parser.Buffer[2]);
            quantity = ModBusReadU16(&Parser.Buffer[4]);
            return ModBusHandleReadInput(slave, startAddress, quantity);

        case MODBUS_FUNC_WRITE_SINGLE_REG:
            startAddress = ModBusReadU16(&Parser.Buffer[2]);
            quantity = ModBusReadU16(&Parser.Buffer[4]);
            return ModBusHandleWriteSingleHolding(slave, startAddress, quantity, broadcast);

        case MODBUS_FUNC_WRITE_MULTIPLE_REG:
            return ModBusHandleWriteMultipleHolding(slave, Parser.Buffer, length, broadcast);

        case MODBUS_FUNC_READ_COILS:
            if (broadcast) {
                return MODBUS_SERVICE_IDLE;
            }
            startAddress = ModBusReadU16(&Parser.Buffer[2]);
            quantity = ModBusReadU16(&Parser.Buffer[4]);
            return ModBusHandleReadCoils(slave, startAddress, quantity);

        case MODBUS_FUNC_WRITE_SINGLE_COIL:
            startAddress = ModBusReadU16(&Parser.Buffer[2]);
            quantity = ModBusReadU16(&Parser.Buffer[4]);
            return ModBusHandleWriteSingleCoil(slave, startAddress, quantity, broadcast);

        case MODBUS_FUNC_WRITE_MULTIPLE_COIL:
            return ModBusHandleWriteMultipleCoils(slave, Parser.Buffer, length, broadcast);

        default:
            if (!broadcast) {
                ModBusSendException(slave, function, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                return MODBUS_SERVICE_RESPONSE_QUEUED;
            }
            return MODBUS_SERVICE_IDLE;
    }
}

static void ModBusSendResponse(uint8_t *buffer, uint8_t length) {
    uint16_t crc = ModBusCrc16(buffer, length);

    for (uint8_t i = 0u; i < length; i = (uint8_t)(i + 1u)) {
        while (!ModBusHwWriteByte(buffer[i])) {
            /* wait for space */
        }
    }

    while (!ModBusHwWriteByte((uint8_t)(crc & 0xFFu))) {
    }
    while (!ModBusHwWriteByte((uint8_t)(crc >> 8))) {
    }
}

static void ModBusSendException(uint8_t address, uint8_t function, uint8_t code) {
    ResponseBuffer[0] = address;
    ResponseBuffer[1] = (uint8_t)(function | 0x80u);
    ResponseBuffer[2] = code;
    ModBusSendResponse(ResponseBuffer, 3u);
}

static uint16_t ModBusReadU16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] << 8) | bytes[1];
}

static void ModBusWriteU16(uint8_t *dest, uint16_t value) {
    dest[0] = (uint8_t)(value >> 8);
    dest[1] = (uint8_t)(value & 0xFFu);
}

static uint8_t ModBusIsBroadcast(uint8_t address) {
    return (address == 0u) ? 1u : 0u;
}

static uint8_t ModBusAccessStatusToException(ModBusAccessStatus status) {
    switch (status) {
        case MODBUS_ACCESS_OK:
            return 0u;
        case MODBUS_ACCESS_INVALID_ADDRESS:
            return MODBUS_EXCEPTION_ILLEGAL_ADDRESS;
        case MODBUS_ACCESS_DENIED:
            return MODBUS_EXCEPTION_ILLEGAL_VALUE;
        case MODBUS_ACCESS_FATAL:
        default:
            return MODBUS_EXCEPTION_SERVER_FAILURE;
    }
}

static ModBusAccessStatus ModBusHoldingRead(uint16_t address, uint16_t *value) {
    if (value == NULL) {
        return MODBUS_ACCESS_FATAL;
    }

    if (PersistentConfigPtr == NULL) {
        return MODBUS_ACCESS_FATAL;
    }

    if (address == MODBUS_REG_HOLDING_DEVICE_ID) {
        *value = PendingConfig.DeviceId;
        return MODBUS_ACCESS_OK;
    }

    if (address == MODBUS_REG_HOLDING_BAUD_ENUM) {
        *value = PendingConfig.BaudRateEnum;
        return MODBUS_ACCESS_OK;
    }

    if (address == MODBUS_REG_HOLDING_RESET_MAGIC) {
        *value = 0u;
        return MODBUS_ACCESS_OK;
    }

    if ((RuntimeConfigPtr != NULL) &&
        (RuntimeConfigPtr->HoldingData != NULL) &&
        (RuntimeConfigPtr->HoldingLength > 0u) &&
        (address >= RuntimeConfigPtr->HoldingBase) &&
        (address < (uint16_t)(RuntimeConfigPtr->HoldingBase + RuntimeConfigPtr->HoldingLength))) {
        uint16_t offset = (uint16_t)(address - RuntimeConfigPtr->HoldingBase);
        *value = RuntimeConfigPtr->HoldingData[offset];
        return MODBUS_ACCESS_OK;
    }

    return modbusHandleHoldingRead(address, value);
}

static ModBusAccessStatus ModBusHoldingWrite(uint16_t address, uint16_t value) {
    if (PersistentConfigPtr == NULL) {
        return MODBUS_ACCESS_FATAL;
    }

    if (address == MODBUS_REG_HOLDING_DEVICE_ID) {
        if ((value == 0u) || (value > 247u)) {
            return MODBUS_ACCESS_DENIED;
        }
        PendingConfig.DeviceId = (uint8_t)value;
        return MODBUS_ACCESS_OK;
    }

    if (address == MODBUS_REG_HOLDING_BAUD_ENUM) {
        if (value > 6u) {
            return MODBUS_ACCESS_DENIED;
        }
        PendingConfig.BaudRateEnum = (uint8_t)value;
        return MODBUS_ACCESS_OK;
    }

    if (address == MODBUS_REG_HOLDING_RESET_MAGIC) {
        if (value == 0xAA55u) {
            PersistentConfigPtr->DeviceId = PendingConfig.DeviceId;
            PersistentConfigPtr->BaudRateEnum = PendingConfig.BaudRateEnum;
            PersistentConfigPtr->ResetCounter = (uint16_t)(PersistentConfigPtr->ResetCounter + 1u);
            modbusConfigChangeResetRequest();
            return MODBUS_ACCESS_OK;
        }
        return MODBUS_ACCESS_DENIED;
    }

    if ((RuntimeConfigPtr != NULL) &&
        (RuntimeConfigPtr->HoldingData != NULL) &&
        (RuntimeConfigPtr->HoldingLength > 0u) &&
        (address >= RuntimeConfigPtr->HoldingBase) &&
        (address < (uint16_t)(RuntimeConfigPtr->HoldingBase + RuntimeConfigPtr->HoldingLength))) {
        uint16_t offset = (uint16_t)(address - RuntimeConfigPtr->HoldingBase);
        RuntimeConfigPtr->HoldingData[offset] = value;
        return MODBUS_ACCESS_OK;
    }

    return modbusHandleHoldingWrite(address, value);
}

static ModBusAccessStatus ModBusInputReadRegister(uint16_t address, uint16_t *value) {
    if ((value == NULL) || (RuntimeConfigPtr == NULL)) {
        return MODBUS_ACCESS_FATAL;
    }

    if (address < (uint16_t)(MODBUS_INPUT_UUID_DEVICE_BASE + MODBUS_UUID_WORDS)) {
        uint16_t offset = (uint16_t)(address - MODBUS_INPUT_UUID_DEVICE_BASE);
        *value = RuntimeConfigPtr->DeviceTypeUuid[offset];
        return MODBUS_ACCESS_OK;
    }

    if ((address >= MODBUS_INPUT_UUID_INSTANCE_BASE) &&
        (address < (uint16_t)(MODBUS_INPUT_UUID_INSTANCE_BASE + MODBUS_UUID_WORDS))) {
        uint16_t offset = (uint16_t)(address - MODBUS_INPUT_UUID_INSTANCE_BASE);
        *value = RuntimeConfigPtr->DeviceInstanceUuid[offset];
        return MODBUS_ACCESS_OK;
    }

    if ((RuntimeConfigPtr->InputData != NULL) &&
        (RuntimeConfigPtr->InputLength > 0u) &&
        (address >= RuntimeConfigPtr->InputBase) &&
        (address < (uint16_t)(RuntimeConfigPtr->InputBase + RuntimeConfigPtr->InputLength))) {
        uint16_t offset = (uint16_t)(address - RuntimeConfigPtr->InputBase);
        *value = RuntimeConfigPtr->InputData[offset];
        return MODBUS_ACCESS_OK;
    }

    return modbusHandleInputRead(address, value);
}

static ModBusAccessStatus ModBusCoilReadBitInternal(uint16_t address, uint8_t *value) {
    if (value == NULL) {
        return MODBUS_ACCESS_FATAL;
    }

    if ((RuntimeConfigPtr != NULL) &&
        (RuntimeConfigPtr->CoilBitmap != NULL) &&
        (RuntimeConfigPtr->CoilBitLength > 0u) &&
        (address >= RuntimeConfigPtr->CoilBase) &&
        (address < (uint16_t)(RuntimeConfigPtr->CoilBase + RuntimeConfigPtr->CoilBitLength))) {
        uint16_t offset = (uint16_t)(address - RuntimeConfigPtr->CoilBase);
        uint16_t byteIndex = (uint16_t)(offset / 8u);
        uint8_t bitIndex = (uint8_t)(offset % 8u);
        const uint16_t requiredBits = RuntimeConfigPtr->CoilBitLength;
        const uint16_t requiredBytes = (uint16_t)((requiredBits + 7u) / 8u);
        if (byteIndex < requiredBytes) {
            uint8_t byte = RuntimeConfigPtr->CoilBitmap[byteIndex];
            *value = (uint8_t)((byte >> bitIndex) & 0x01u);
            return MODBUS_ACCESS_OK;
        }
    }

    return modbusHandleCoilRead(address, value);
}

static ModBusAccessStatus ModBusCoilWriteBitInternal(uint16_t address, uint8_t value) {
    if ((RuntimeConfigPtr != NULL) &&
        (RuntimeConfigPtr->CoilBitmap != NULL) &&
        (RuntimeConfigPtr->CoilBitLength > 0u) &&
        (address >= RuntimeConfigPtr->CoilBase) &&
        (address < (uint16_t)(RuntimeConfigPtr->CoilBase + RuntimeConfigPtr->CoilBitLength))) {
        uint16_t offset = (uint16_t)(address - RuntimeConfigPtr->CoilBase);
        uint16_t byteIndex = (uint16_t)(offset / 8u);
        uint8_t bitIndex = (uint8_t)(offset % 8u);
        const uint16_t requiredBits = RuntimeConfigPtr->CoilBitLength;
        const uint16_t requiredBytes = (uint16_t)((requiredBits + 7u) / 8u);
        if (byteIndex < requiredBytes) {
            if (value != 0u) {
                RuntimeConfigPtr->CoilBitmap[byteIndex] |= (uint8_t)(1u << bitIndex);
            } else {
                RuntimeConfigPtr->CoilBitmap[byteIndex] &= (uint8_t)~(1u << bitIndex);
            }
            return MODBUS_ACCESS_OK;
        }
    }

    return modbusHandleCoilWrite(address, value);
}

static ModBusServiceStatus ModBusHandleReadHolding(uint8_t slave, uint16_t start, uint16_t quantity) {
    if ((quantity == 0u) || (quantity > MODBUS_MAX_HOLDING_PER_REQUEST)) {
        ModBusSendException(slave, MODBUS_FUNC_READ_HOLDING, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        return MODBUS_SERVICE_ERROR;
    }

    uint8_t byteCount = (uint8_t)(quantity * 2u);
    ResponseBuffer[0] = slave;
    ResponseBuffer[1] = MODBUS_FUNC_READ_HOLDING;
    ResponseBuffer[2] = byteCount;
    for (uint16_t i = 0u; i < quantity; i = (uint16_t)(i + 1u)) {
        uint16_t value;
        ModBusAccessStatus status = ModBusHoldingRead((uint16_t)(start + i), &value);
        if (status != MODBUS_ACCESS_OK) {
            ModBusSendException(slave, MODBUS_FUNC_READ_HOLDING, ModBusAccessStatusToException(status));
            return MODBUS_SERVICE_ERROR;
        }
        ModBusWriteU16(&ResponseBuffer[3u + (i * 2u)], value);
    }

    ModBusSendResponse(ResponseBuffer, (uint8_t)(3u + byteCount));
    return MODBUS_SERVICE_RESPONSE_QUEUED;
}

static ModBusServiceStatus ModBusHandleReadInput(uint8_t slave, uint16_t start, uint16_t quantity) {
    if ((quantity == 0u) || (quantity > MODBUS_MAX_INPUT_PER_REQUEST)) {
        ModBusSendException(slave, MODBUS_FUNC_READ_INPUT, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        return MODBUS_SERVICE_ERROR;
    }

    uint8_t byteCount = (uint8_t)(quantity * 2u);
    ResponseBuffer[0] = slave;
    ResponseBuffer[1] = MODBUS_FUNC_READ_INPUT;
    ResponseBuffer[2] = byteCount;

    for (uint16_t i = 0u; i < quantity; i = (uint16_t)(i + 1u)) {
        uint16_t value;
        ModBusAccessStatus status = ModBusInputReadRegister((uint16_t)(start + i), &value);
        if (status != MODBUS_ACCESS_OK) {
            ModBusSendException(slave, MODBUS_FUNC_READ_INPUT, ModBusAccessStatusToException(status));
            return MODBUS_SERVICE_ERROR;
        }
        ModBusWriteU16(&ResponseBuffer[3u + (i * 2u)], value);
    }

    ModBusSendResponse(ResponseBuffer, (uint8_t)(3u + byteCount));
    return MODBUS_SERVICE_RESPONSE_QUEUED;
}

static ModBusServiceStatus ModBusHandleWriteSingleHolding(uint8_t slave, uint16_t address, uint16_t value, uint8_t broadcast) {
    ModBusAccessStatus status = ModBusHoldingWrite(address, value);
    if (status != MODBUS_ACCESS_OK) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_SINGLE_REG, ModBusAccessStatusToException(status));
            return MODBUS_SERVICE_ERROR;
        }
        return MODBUS_SERVICE_IDLE;
    }

    if (!broadcast) {
        memcpy(ResponseBuffer, Parser.Buffer, 6u);
        ModBusSendResponse(ResponseBuffer, 6u);
        return MODBUS_SERVICE_RESPONSE_QUEUED;
    }

    return MODBUS_SERVICE_IDLE;
}

static ModBusServiceStatus ModBusHandleWriteMultipleHolding(uint8_t slave, const uint8_t *frame, uint8_t length, uint8_t broadcast) {
    if (length < 9u) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_REG, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        }
        return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
    }

    uint16_t start = ModBusReadU16(&frame[2]);
    uint16_t quantity = ModBusReadU16(&frame[4]);
    uint8_t byteCount = frame[6];

    if ((quantity == 0u) || (quantity > MODBUS_MAX_HOLDING_PER_REQUEST) ||
        (byteCount != (uint8_t)(quantity * 2u)) || ((7u + byteCount + 2u) > length)) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_REG, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        }
        return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
    }

    const uint8_t *payload = &frame[7];

    for (uint16_t i = 0u; i < quantity; i = (uint16_t)(i + 1u)) {
        uint16_t value = ModBusReadU16(&payload[i * 2u]);
        ModBusAccessStatus status = ModBusHoldingWrite((uint16_t)(start + i), value);
        if (status != MODBUS_ACCESS_OK) {
            if (!broadcast) {
                ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_REG, ModBusAccessStatusToException(status));
            }
            return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
        }
    }

    if (!broadcast) {
        ResponseBuffer[0] = slave;
        ResponseBuffer[1] = MODBUS_FUNC_WRITE_MULTIPLE_REG;
        ModBusWriteU16(&ResponseBuffer[2], start);
        ModBusWriteU16(&ResponseBuffer[4], quantity);
        ModBusSendResponse(ResponseBuffer, 6u);
        return MODBUS_SERVICE_RESPONSE_QUEUED;
    }

    return MODBUS_SERVICE_IDLE;
}

static ModBusServiceStatus ModBusHandleReadCoils(uint8_t slave, uint16_t start, uint16_t quantity) {
    if ((quantity == 0u) || (quantity > MODBUS_MAX_COILS_PER_REQUEST)) {
        ModBusSendException(slave, MODBUS_FUNC_READ_COILS, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        return MODBUS_SERVICE_ERROR;
    }

    uint8_t byteCount = (uint8_t)((quantity + 7u) / 8u);
    ResponseBuffer[0] = slave;
    ResponseBuffer[1] = MODBUS_FUNC_READ_COILS;
    ResponseBuffer[2] = byteCount;
    memset(&ResponseBuffer[3], 0, byteCount);

    for (uint16_t i = 0u; i < quantity; i = (uint16_t)(i + 1u)) {
        uint8_t bit;
        ModBusAccessStatus status = ModBusCoilReadBitInternal((uint16_t)(start + i), &bit);
        if (status != MODBUS_ACCESS_OK) {
            ModBusSendException(slave, MODBUS_FUNC_READ_COILS, ModBusAccessStatusToException(status));
            return MODBUS_SERVICE_ERROR;
        }
        uint16_t byteIndex = (uint16_t)(i / 8u);
        uint8_t bitIndex = (uint8_t)(i % 8u);
        if (bit != 0u) {
            ResponseBuffer[3u + byteIndex] |= (uint8_t)(1u << bitIndex);
        }
    }

    ModBusSendResponse(ResponseBuffer, (uint8_t)(3u + byteCount));
    return MODBUS_SERVICE_RESPONSE_QUEUED;
}

static ModBusServiceStatus ModBusHandleWriteSingleCoil(uint8_t slave, uint16_t address, uint16_t value, uint8_t broadcast) {
    uint8_t bitValue;
    if (value == 0xFF00u) {
        bitValue = 1u;
    } else if (value == 0x0000u) {
        bitValue = 0u;
    } else {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EXCEPTION_ILLEGAL_VALUE);
            return MODBUS_SERVICE_ERROR;
        }
        return MODBUS_SERVICE_IDLE;
    }

    ModBusAccessStatus status = ModBusCoilWriteBitInternal(address, bitValue);
    if (status != MODBUS_ACCESS_OK) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_SINGLE_COIL, ModBusAccessStatusToException(status));
            return MODBUS_SERVICE_ERROR;
        }
        return MODBUS_SERVICE_IDLE;
    }

    if (!broadcast) {
        memcpy(ResponseBuffer, Parser.Buffer, 6u);
        ModBusSendResponse(ResponseBuffer, 6u);
        return MODBUS_SERVICE_RESPONSE_QUEUED;
    }

    return MODBUS_SERVICE_IDLE;
}

static ModBusServiceStatus ModBusHandleWriteMultipleCoils(uint8_t slave, const uint8_t *frame, uint8_t length, uint8_t broadcast) {
    if (length < 9u) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_COIL, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        }
        return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
    }

    uint16_t start = ModBusReadU16(&frame[2]);
    uint16_t quantity = ModBusReadU16(&frame[4]);
    uint8_t byteCount = frame[6];

    if ((quantity == 0u) || (quantity > MODBUS_MAX_COILS_PER_REQUEST) ||
        (byteCount != (uint8_t)((quantity + 7u) / 8u)) ||
        ((7u + byteCount + 2u) > length)) {
        if (!broadcast) {
            ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_COIL, MODBUS_EXCEPTION_ILLEGAL_VALUE);
        }
        return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
    }

    const uint8_t *payload = &frame[7];

    for (uint16_t i = 0u; i < quantity; i = (uint16_t)(i + 1u)) {
        uint16_t byteIndex = (uint16_t)(i / 8u);
        uint8_t bitIndex = (uint8_t)(i % 8u);
        uint8_t bit = (uint8_t)((payload[byteIndex] >> bitIndex) & 0x01u);
        ModBusAccessStatus status = ModBusCoilWriteBitInternal((uint16_t)(start + i), bit);
        if (status != MODBUS_ACCESS_OK) {
            if (!broadcast) {
                ModBusSendException(slave, MODBUS_FUNC_WRITE_MULTIPLE_COIL, ModBusAccessStatusToException(status));
            }
            return broadcast ? MODBUS_SERVICE_IDLE : MODBUS_SERVICE_ERROR;
        }
    }

    if (!broadcast) {
        ResponseBuffer[0] = slave;
        ResponseBuffer[1] = MODBUS_FUNC_WRITE_MULTIPLE_COIL;
        ModBusWriteU16(&ResponseBuffer[2], start);
        ModBusWriteU16(&ResponseBuffer[4], quantity);
        ModBusSendResponse(ResponseBuffer, 6u);
        return MODBUS_SERVICE_RESPONSE_QUEUED;
    }

    return MODBUS_SERVICE_IDLE;
}
