#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

#include "modbus_core.h"
#include "modbus_config.h"
#include "modbus_registers.h"

#define EXAMPLE_STORE_VERSION        	1u
#define EXAMPLE_HOLDING_BASE        	16u
#define EXAMPLE_HOLDING_COUNT        	4u
#define EXAMPLE_INPUT_BASE          	32u
#define EXAMPLE_INPUT_COUNT          	4u

#define EXAMPLE_LED_DDR   		DDRB
#define EXAMPLE_LED_PORT  		PORTB
#define EXAMPLE_LED_PIN   		PB7

#define EXAMPLE_SWITCH_DDR  		DDRF
#define EXAMPLE_SWITCH_PORT 		PORTF
#define EXAMPLE_SWITCH_PINR 		PINF
#define EXAMPLE_SWITCH_PIN  		PF0

#define EXAMPLE_ADC_CHANNEL 		0u

typedef struct {
    uint8_t Version;
    uint8_t Reserved;
    uint16_t Crc;
    ModBusPersistentConfig Config;
} ExamplePersistentStore;

static ExamplePersistentStore EEMEM ExampleEepromStore;
static ExamplePersistentStore ExampleStore;

static uint16_t ExampleHoldingData[EXAMPLE_HOLDING_COUNT];
static uint16_t ExampleInputData[EXAMPLE_INPUT_COUNT];

static volatile uint8_t ExampleResetPending = 0u;

static uint16_t ExampleCrc16(const uint8_t *data, uint16_t length);
static void ExampleLoadPersistent(void);
static void ExampleQueuePersistentSave(void);
static void ExamplePersistAndReset(void);
static void ExampleInitGpio(void);
static void ExampleInitAdc(void);
static uint16_t ExampleReadAdc(void);
static void ExampleUpdateInputs(void);
static void ExampleApplyOutputs(void);
static void ExampleSetLed(uint8_t state);
static uint8_t ExampleGetLed(void);
static uint8_t ExampleReadSwitch(void);

int main(void) {
    ExampleLoadPersistent();

    /*
        ModBus runtime configuration
    */
    ModBusRuntimeConfig runtime;
    ModBusLoadDefaultRuntime(&runtime);
    runtime.HoldingBase 	= EXAMPLE_HOLDING_BASE;
    runtime.HoldingLength 	= EXAMPLE_HOLDING_COUNT;
    runtime.HoldingData 	= ExampleHoldingData;
    runtime.InputBase 		= EXAMPLE_INPUT_BASE;
    runtime.InputLength 	= EXAMPLE_INPUT_COUNT;
    runtime.InputData 		= ExampleInputData;
    runtime.CoilBase 		= 0u;
    runtime.CoilBitLength 	= 0u;
    runtime.CoilBitmap 		= NULL;

    /*
        Zero holding register status and input register status
    */
    memset(ExampleHoldingData, 0, sizeof(ExampleHoldingData));
    memset(ExampleInputData, 0, sizeof(ExampleInputData));

    /*
        Initialize GPIOs and ADCs
    */
    ExampleInitGpio();
    ExampleInitAdc();

    /* Flash LED 8 times so we have a visual indicator (Arduino Mega 2560 ...) */
    uint8_t ctr = 8;
    while(ctr > 0) {
        ExampleSetLed(1u);
        _delay_ms(100u);
        ExampleSetLed(0u);
        _delay_ms(100u);
        ctr = ctr - 1;
    }

    /* Initialize ModBus module */
    ModBusInit(&ExampleStore.Config, &runtime);

    /* Enable interrupts, start infinite loop */
    sei();
    for (;;) {
        ModBusService(); /* Periodic function call to process data ... */
	#if !MODBUS_ENABLE_INTERNAL_TICK
	    /*
	        If we do the ticks (the ModBus module does not control Timers) we have to call
		the NotifyTick method. Usually we should do this from our timer callback but somehow
		hack'ish we can also do this here ... the timeouts won't be accurate though
	    */
            ModBusNotifyTick();
	#endif

        /*
 	    Update input registers and output registers if they changed
	*/
        ExampleUpdateInputs();
        ExampleApplyOutputs();

	/*
	    If a reset if pending we execute it (incl. EEPROM persisting)
	*/
        ExamplePersistAndReset();
    }
}

/*
    CRC16 method.
   
    Maybe we can move this into the library? This is used for EEPROM checksumming
*/
static uint16_t ExampleCrc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFFu;

    for (uint16_t i = 0u; i < length; i = (uint16_t)(i + 1u)) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; bit = (uint8_t)(bit + 1u)) {
            if ((crc & 0x0001u) != 0u) {
                crc = (crc >> 1) ^ 0xA001u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/*
    Load configuration data from EEPROM, check CRC and if invalid we load the default
    configuration & overwrite (this solves the bootstrapping problem
*/
static void ExampleLoadPersistent(void) {
    eeprom_read_block(&ExampleStore, &ExampleEepromStore, sizeof(ExampleStore));

    uint16_t expectedCrc = ExampleCrc16((const uint8_t *)&ExampleStore.Config, sizeof(ModBusPersistentConfig));
    if ((ExampleStore.Version != EXAMPLE_STORE_VERSION) || (ExampleStore.Crc != expectedCrc)) {
        ModBusLoadDefaultPersistent(&ExampleStore.Config);
        ExampleStore.Version = EXAMPLE_STORE_VERSION;
        ExampleStore.Reserved = 0u;
        ExampleStore.Crc = ExampleCrc16((const uint8_t *)&ExampleStore.Config, sizeof(ModBusPersistentConfig));
        eeprom_update_block(&ExampleStore, &ExampleEepromStore, sizeof(ExampleStore));
    }
}

/*
    Write the EEPROM after calculating the CRC16 checksum
*/
static void ExampleQueuePersistentSave(void) {
    ExampleStore.Crc = ExampleCrc16((const uint8_t *)&ExampleStore.Config, sizeof(ModBusPersistentConfig));
    eeprom_update_block(&ExampleStore, &ExampleEepromStore, sizeof(ExampleStore));
}

/*
    If we have an enqueued persist and reset request disable interrupts
    and execute.
*/
static void ExamplePersistAndReset(void) {
    if (ExampleResetPending != 0u) {
	cli();
        ExampleResetPending = 0u;
        ExampleQueuePersistentSave();
        wdt_enable(WDTO_15MS);
	/* Infinite loop - we will reset by the WDT */
        for (;;) { }
    }
}

/*
    Initialize GPIO directions and status ...
*/
static void ExampleInitGpio(void) {
    EXAMPLE_LED_DDR |= _BV(EXAMPLE_LED_PIN);
    ExampleSetLed(0u);

    EXAMPLE_SWITCH_DDR &= (uint8_t)~_BV(EXAMPLE_SWITCH_PIN);
    EXAMPLE_SWITCH_PORT |= _BV(EXAMPLE_SWITCH_PIN);
}

/*
    Initialize the analog to digital converter
*/
static void ExampleInitAdc(void) {
    ADMUX = (uint8_t)((1u << REFS0) | (EXAMPLE_ADC_CHANNEL & 0x07u));
    ADCSRA = (uint8_t)(_BV(ADEN) | _BV(ADPS2) | _BV(ADPS1));
}

/*
    Single sample read by ADC
*/
static uint16_t ExampleReadAdc(void) {
    ADCSRA |= _BV(ADSC);
    while ((ADCSRA & _BV(ADSC)) != 0u) { }
    return ADC;
}

/*
    Read GPIO input ...
*/
static uint8_t ExampleReadSwitch(void) {
    return ((EXAMPLE_SWITCH_PINR & _BV(EXAMPLE_SWITCH_PIN)) == 0u) ? 1u : 0u;
}

/*
    Read inputs (digital and analog) and update the input register values
*/
static void ExampleUpdateInputs(void) {
    ExampleInputData[0] = (uint16_t)ExampleReadSwitch();
    ExampleInputData[1] = ExampleReadAdc();
}

/*
    Set outputs depending on the values of our holding registers
*/
static void ExampleApplyOutputs(void) {
    static uint16_t previous = 0xFFFFu;
    uint16_t current = ExampleHoldingData[0] ? 1u : 0u;
    if (current != previous) {
        ExampleSetLed((uint8_t)current);
        previous = current;
    }
}

/*
    We could inline this
*/
static void ExampleSetLed(uint8_t state) {
    if (state != 0u) {
        EXAMPLE_LED_PORT |= _BV(EXAMPLE_LED_PIN);
    } else {
        EXAMPLE_LED_PORT &= (uint8_t)~_BV(EXAMPLE_LED_PIN);
    }
}

/*
    Read current status of the output
*/
static uint8_t ExampleGetLed(void) {
    return (EXAMPLE_LED_PORT & _BV(EXAMPLE_LED_PIN)) ? 1u : 0u;
}

void modbusConfigChangeResetRequest(void) {
    ExampleResetPending = 1u;
}

/*
   Handle "coil" read and writes (i.e. LED status change)
*/
ModBusAccessStatus modbusHandleCoilRead(uint16_t address, uint8_t *value) {
    if ((address == 0u) && (value != NULL)) {
        *value = ExampleGetLed();
        return MODBUS_ACCESS_OK;
    }
    return MODBUS_ACCESS_INVALID_ADDRESS;
}

ModBusAccessStatus modbusHandleCoilWrite(uint16_t address, uint8_t value) {
    if (address == 0u) {
        uint8_t normalized = (value != 0u) ? 1u : 0u;
        ExampleHoldingData[0] = normalized;
        ExampleSetLed(normalized);
        return MODBUS_ACCESS_OK;
    }
    return MODBUS_ACCESS_INVALID_ADDRESS;
}
