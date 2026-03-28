#include "modbus_hw.h"

#include <avr/interrupt.h>
#include <stddef.h>

#if MODBUS_ENABLE_INTERNAL_TICK
	extern void ModBusNotifyTick(void);
#endif

static volatile uint8_t RxStorage[MODBUS_UART_RX_BUFFER_SIZE];
static volatile uint8_t TxStorage[MODBUS_UART_TX_BUFFER_SIZE];

static ModBusRingBuffer RxBuffer = {0u, 0u, 0u, MODBUS_UART_RX_BUFFER_SIZE, RxStorage};
static ModBusRingBuffer TxBuffer = {0u, 0u, 0u, MODBUS_UART_TX_BUFFER_SIZE, TxStorage};
static volatile uint8_t ModBusTxActive = 0u;


#if MODBUS_UART_INSTANCE == 0
    #if defined(UCSR0A)
        #define MODBUS_UCSRnA UCSR0A
        #define MODBUS_UCSRnB UCSR0B
        #define MODBUS_UCSRnC UCSR0C
        #define MODBUS_UDRn   UDR0
        #define MODBUS_UBRRnH UBRR0H
        #define MODBUS_UBRRnL UBRR0L
        #define MODBUS_RXENn  RXEN0
        #define MODBUS_TXENn  TXEN0
        #define MODBUS_RXCIEn RXCIE0
        #define MODBUS_TXCIEn TXCIE0
        #define MODBUS_UDRIEn UDRIE0
        #define MODBUS_U2Xn   U2X0
        #define MODBUS_UCSZ1_BIT UCSZ01
        #define MODBUS_UCSZ0_BIT UCSZ00
        #define MODBUS_TXCn_BIT TXC0
        #if defined(USART0_RX_vect)
            #define MODBUS_USART_RX_VECTOR   USART0_RX_vect
            #define MODBUS_USART_UDRE_VECTOR USART0_UDRE_vect
        #else
            #define MODBUS_USART_RX_VECTOR   USART_RX_vect
            #define MODBUS_USART_UDRE_VECTOR USART_UDRE_vect
        #endif
    #elif defined(UCSRA)
        #define MODBUS_UCSRnA UCSRA
        #define MODBUS_UCSRnB UCSRB
        #define MODBUS_UCSRnC UCSRC
        #define MODBUS_UDRn   UDR
        #define MODBUS_UBRRnH UBRRH
        #define MODBUS_UBRRnL UBRRL
        #define MODBUS_RXENn  RXEN
        #define MODBUS_TXENn  TXEN
        #define MODBUS_RXCIEn RXCIE
        #define MODBUS_TXCIEn TXCIE
        #define MODBUS_UDRIEn UDRIE
        #define MODBUS_U2Xn   U2X
        #define MODBUS_UCSZ1_BIT UCSZ1
        #define MODBUS_UCSZ0_BIT UCSZ00
        #define MODBUS_USART_RX_VECTOR   USART_RX_vect
        #define MODBUS_USART_UDRE_VECTOR USART_UDRE_vect
    #else
        #error "USART0 register set not available on this MCU"
    #endif
#elif MODBUS_UART_INSTANCE == 1
    #if defined(UCSR1A)
        #define MODBUS_UCSRnA UCSR1A
        #define MODBUS_UCSRnB UCSR1B
        #define MODBUS_UCSRnC UCSR1C
        #define MODBUS_UDRn   UDR1
        #define MODBUS_UBRRnH UBRR1H
        #define MODBUS_UBRRnL UBRR1L
        #define MODBUS_RXENn  RXEN1
        #define MODBUS_TXENn  TXEN1
        #define MODBUS_RXCIEn RXCIE1
        #define MODBUS_TXCIEn TXCIE1
        #define MODBUS_UDRIEn UDRIE1
        #define MODBUS_U2Xn   U2X1
        #define MODBUS_UCSZ1_BIT UCSZ11
        #define MODBUS_UCSZ0_BIT UCSZ10
        #define MODBUS_TXCn_BIT TXC1
        #define MODBUS_USART_RX_VECTOR   USART1_RX_vect
        #define MODBUS_USART_UDRE_VECTOR USART1_UDRE_vect
    #else
        #error "USART1 not available on this MCU"
    #endif
#elif MODBUS_UART_INSTANCE == 2
    #if defined(UCSR2A)
        #define MODBUS_UCSRnA UCSR2A
        #define MODBUS_UCSRnB UCSR2B
        #define MODBUS_UCSRnC UCSR2C
        #define MODBUS_UDRn   UDR2
        #define MODBUS_UBRRnH UBRR2H
        #define MODBUS_UBRRnL UBRR2L
        #define MODBUS_RXENn  RXEN2
        #define MODBUS_TXENn  TXEN2
        #define MODBUS_RXCIEn RXCIE2
        #define MODBUS_TXCIEn TXCIE2
        #define MODBUS_UDRIEn UDRIE2
        #define MODBUS_U2Xn   U2X2
        #define MODBUS_UCSZ1_BIT UCSZ21
        #define MODBUS_UCSZ0_BIT UCSZ20
        #define MODBUS_TXCn_BIT TXC2
        #define MODBUS_USART_RX_VECTOR   USART2_RX_vect
        #define MODBUS_USART_UDRE_VECTOR USART2_UDRE_vect
    #else
        #error "USART2 not available on this MCU"
    #endif
#elif MODBUS_UART_INSTANCE == 3
    #if defined(UCSR3A)
        #define MODBUS_UCSRnA UCSR3A
        #define MODBUS_UCSRnB UCSR3B
        #define MODBUS_UCSRnC UCSR3C
        #define MODBUS_UDRn   UDR3
        #define MODBUS_UBRRnH UBRR3H
        #define MODBUS_UBRRnL UBRR3L
        #define MODBUS_RXENn  RXEN3
        #define MODBUS_TXENn  TXEN3
        #define MODBUS_RXCIEn RXCIE3
        #define MODBUS_TXCIEn TXCIE3
        #define MODBUS_UDRIEn UDRIE3
        #define MODBUS_U2Xn   U2X3
        #define MODBUS_UCSZ1_BIT UCSZ31
        #define MODBUS_UCSZ0_BIT UCSZ30
        #define MODBUS_TXCn_BIT TXC3
        #define MODBUS_USART_RX_VECTOR   USART3_RX_vect
        #define MODBUS_USART_UDRE_VECTOR USART3_UDRE_vect
    #else
        #error "USART3 not available on this MCU"
    #endif
#else
    #error "Unsupported MODBUS_UART_INSTANCE value"
#endif

typedef struct {
    uint16_t Ubrr;
    uint8_t UseDoubleSpeed;
} ModBusBaudConfig;

#if MODBUS_ENABLE_INTERNAL_TICK
	#if MODBUS_TICK_TIMER == 0u
		#if MODBUS_TICK_TIMER0_PRESCALER == 1u
			#define MODBUS_TICK_TIMER0_CS_BITS _BV(CS00)
		#elif MODBUS_TICK_TIMER0_PRESCALER == 8u
			#define MODBUS_TICK_TIMER0_CS_BITS _BV(CS01)
		#elif MODBUS_TICK_TIMER0_PRESCALER == 64u
			#define MODBUS_TICK_TIMER0_CS_BITS (_BV(CS01) | _BV(CS00))
		#elif MODBUS_TICK_TIMER0_PRESCALER == 256u
			#define MODBUS_TICK_TIMER0_CS_BITS _BV(CS02)
		#elif MODBUS_TICK_TIMER0_PRESCALER == 1024u
			#define MODBUS_TICK_TIMER0_CS_BITS (_BV(CS02) | _BV(CS00))
		#else
			#error "Unsupported Timer0 prescaler"
		#endif
		#define MODBUS_TICK_TIMER0_COMPARE_VALUE (uint8_t)((((uint32_t)F_CPU / (uint32_t)MODBUS_TICK_TIMER0_PRESCALER) / (uint32_t)MODBUS_TICK_FREQUENCY_HZ) - 1u)
	#elif MODBUS_TICK_TIMER == 1u
		#if MODBUS_TICK_TIMER1_PRESCALER == 1u
			#define MODBUS_TICK_TIMER1_CS_BITS _BV(CS10)
		#elif MODBUS_TICK_TIMER1_PRESCALER == 8u
			#define MODBUS_TICK_TIMER1_CS_BITS _BV(CS11)
		#elif MODBUS_TICK_TIMER1_PRESCALER == 64u
			#define MODBUS_TICK_TIMER1_CS_BITS (_BV(CS11) | _BV(CS10))
		#elif MODBUS_TICK_TIMER1_PRESCALER == 256u
			#define MODBUS_TICK_TIMER1_CS_BITS _BV(CS12)
		#elif MODBUS_TICK_TIMER1_PRESCALER == 1024u
			#define MODBUS_TICK_TIMER1_CS_BITS (_BV(CS12) | _BV(CS10))
		#else
			#error "Unsupported Timer1 prescaler"
		#endif
		#define MODBUS_TICK_TIMER1_COMPARE_VALUE (uint16_t)((((uint32_t)F_CPU / (uint32_t)MODBUS_TICK_TIMER1_PRESCALER) / (uint32_t)MODBUS_TICK_FREQUENCY_HZ) - 1u)
	#else
		#error "Unsupported MODBUS_TICK_TIMER selection"
	#endif

	static void ModBusHwInitTickTimer(void) {
		#if MODBUS_TICK_TIMER == 0u
			TCCR0A = (uint8_t)_BV(WGM01);
			TCCR0B &= (uint8_t)~(_BV(WGM02) | _BV(CS02) | _BV(CS01) | _BV(CS00));
			TCCR0B |= MODBUS_TICK_TIMER0_CS_BITS;
			OCR0A = MODBUS_TICK_TIMER0_COMPARE_VALUE;
			TIMSK0 |= _BV(OCIE0A);
		#elif MODBUS_TICK_TIMER == 1u
			TCCR1A &= (uint8_t)~(_BV(WGM10) | _BV(WGM11));
			TCCR1B &= (uint8_t)~(_BV(WGM13) | _BV(CS12) | _BV(CS11) | _BV(CS10));
			TCCR1B |= _BV(WGM12) | MODBUS_TICK_TIMER1_CS_BITS;
			OCR1A = MODBUS_TICK_TIMER1_COMPARE_VALUE;
			TIMSK1 |= _BV(OCIE1A);
		#endif
	}
#endif

static inline void ModBusHwResetBuffer(ModBusRingBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    buffer->Head = 0u;
    buffer->Tail = 0u;
    buffer->Count = 0u;
}

static void ModBusHwSetDriverEnable(uint8_t enable) {
    #if MODBUS_MAX485_DE_ACTIVE_HIGH
    	if (enable != 0u) {
	        MODBUS_MAX485_DE_PORT |= _BV(MODBUS_MAX485_DE_PIN);
	    } else {
	        MODBUS_MAX485_DE_PORT &= (uint8_t)~_BV(MODBUS_MAX485_DE_PIN);
	    }
   #else
	if (enable != 0u) {
		MODBUS_MAX485_DE_PORT &= (uint8_t)~_BV(MODBUS_MAX485_DE_PIN);
    	} else {
        	MODBUS_MAX485_DE_PORT |= _BV(MODBUS_MAX485_DE_PIN);
    	}
    #endif
}

static ModBusBaudConfig ModBusHwBaudEnumToConfig(uint8_t baudEnum) {
    ModBusBaudConfig cfg;
    cfg.UseDoubleSpeed = 0u;

    switch (baudEnum) {
        case 0u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 2400UL)) - 1UL);
            break;
        case 1u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 4800UL)) - 1UL);
            break;
        case 2u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 9600UL)) - 1UL);
            break;
        case 3u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 19200UL)) - 1UL);
            break;
        case 4u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 38400UL)) - 1UL);
            break;
        case 5u:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 57600UL)) - 1UL);
            break;
        case 6u:
            cfg.Ubrr = (uint16_t)((F_CPU / (8UL * 115200UL)) - 1UL);
            cfg.UseDoubleSpeed = 1u;
            break;
        default:
            cfg.Ubrr = (uint16_t)((F_CPU / (16UL * 9600UL)) - 1UL);
            break;
    }

    return cfg;
}

void ModBusHwInit(
     ModBusPersistentConfig *persistent,
     const ModBusRuntimeConfig *runtime
) {
    (void)runtime;

    ModBusHwResetBuffer(&RxBuffer);
    ModBusHwResetBuffer(&TxBuffer);

    MODBUS_MAX485_DE_DDR |= _BV(MODBUS_MAX485_DE_PIN);
    ModBusHwDisableTransmitter();

    uint8_t baudEnum = (persistent != NULL) ? persistent->BaudRateEnum : MODBUS_DEFAULT_BAUD_ENUM;
    ModBusBaudConfig baudCfg = ModBusHwBaudEnumToConfig(baudEnum);

    MODBUS_UBRRnH = (uint8_t)(baudCfg.Ubrr >> 8);
    MODBUS_UBRRnL = (uint8_t)(baudCfg.Ubrr & 0xFFu);

    if (baudCfg.UseDoubleSpeed != 0u) {
        MODBUS_UCSRnA |= _BV(MODBUS_U2Xn);
    } else {
        MODBUS_UCSRnA &= (uint8_t)~_BV(MODBUS_U2Xn);
    }

    MODBUS_UCSRnB = (uint8_t)(_BV(MODBUS_RXENn) | _BV(MODBUS_TXENn) | _BV(MODBUS_RXCIEn));
    MODBUS_UCSRnC = (uint8_t)(_BV(MODBUS_UCSZ1_BIT) | _BV(MODBUS_UCSZ0_BIT));

    #if MODBUS_ENABLE_INTERNAL_TICK
        ModBusHwInitTickTimer();
    #endif
}

void ModBusHwEnableTransmitter(void) {
    ModBusHwSetDriverEnable(1u);
    MODBUS_UCSRnA |= _BV(MODBUS_TXCn_BIT);
    ModBusTxActive = 1u;
    MODBUS_UCSRnB |= _BV(MODBUS_UDRIEn);
}

void ModBusHwDisableTransmitter(void) {
    MODBUS_UCSRnB &= (uint8_t)~_BV(MODBUS_UDRIEn);
    if (ModBusTxActive != 0u) {
        while ((MODBUS_UCSRnA & _BV(MODBUS_TXCn_BIT)) == 0u) { }
        MODBUS_UCSRnA |= _BV(MODBUS_TXCn_BIT);
        ModBusTxActive = 0u;
    }
    ModBusHwSetDriverEnable(0u);
}

uint8_t ModBusHwRxAvailable(void) {
    return RxBuffer.Count;
}

uint8_t ModBusHwTxSpace(void) {
    return (uint8_t)(TxBuffer.Size - TxBuffer.Count);
}

int ModBusHwReadByte(uint8_t *byte) {
    uint8_t success = 0u;
    uint8_t sreg = SREG;
    cli();
    if ((byte != NULL) && (RxBuffer.Count > 0u)) {
        *byte = RxBuffer.Buffer[RxBuffer.Tail];
        RxBuffer.Tail = (uint8_t)((RxBuffer.Tail + 1u) % RxBuffer.Size);
        RxBuffer.Count = (uint8_t)(RxBuffer.Count - 1u);
        success = 1u;
    }
    SREG = sreg;
    return success;
}

int ModBusHwWriteByte(uint8_t byte) {
    uint8_t success = 0u;
    uint8_t sreg = SREG;
    cli();
    if (TxBuffer.Count < TxBuffer.Size) {
        TxBuffer.Buffer[TxBuffer.Head] = byte;
        TxBuffer.Head = (uint8_t)((TxBuffer.Head + 1u) % TxBuffer.Size);
        TxBuffer.Count = (uint8_t)(TxBuffer.Count + 1u);
        success = 1u;
    }
    SREG = sreg;

    if (success != 0u) {
        ModBusHwEnableTransmitter();
    }
    return success;
}

void ModBusHwFlushTransmit(void) {
    while (TxBuffer.Count != 0u) {
        /* Wait for ISR to drain buffer. */
    }
    ModBusHwDisableTransmitter();
}

ISR(MODBUS_USART_RX_VECTOR) {
    uint8_t data = MODBUS_UDRn;
    if (RxBuffer.Count < RxBuffer.Size) {
        RxBuffer.Buffer[RxBuffer.Head] = data;
        RxBuffer.Head = (uint8_t)((RxBuffer.Head + 1u) % RxBuffer.Size);
        RxBuffer.Count = (uint8_t)(RxBuffer.Count + 1u);
    }
}

ISR(MODBUS_USART_UDRE_VECTOR) {
    if (TxBuffer.Count == 0u) {
        ModBusHwDisableTransmitter();
        return;
    }

    uint8_t tail = TxBuffer.Tail;
    MODBUS_UDRn = TxBuffer.Buffer[tail];
    TxBuffer.Tail = (uint8_t)((tail + 1u) % TxBuffer.Size);
    TxBuffer.Count = (uint8_t)(TxBuffer.Count - 1u);
}

#if MODBUS_ENABLE_INTERNAL_TICK && (MODBUS_TICK_TIMER == 0u)
    ISR(TIMER0_COMPA_vect) {
    	ModBusNotifyTick();
    }
#elif MODBUS_ENABLE_INTERNAL_TICK && (MODBUS_TICK_TIMER == 1u)
    ISR(TIMER1_COMPA_vect) {
	    ModBusNotifyTick();
    }
#endif
