AVR_PREFIX = avr
CC := $(AVR_PREFIX)-gcc
OBJCOPY := $(AVR_PREFIX)-objcopy
MCU ?= atmega328p
F_CPU ?= 16000000UL
TARGET ?= basic
FLASHMETHOD ?= wiring
FLASHBAUD ?= 115200
FLASHDEV ?= /dev/ttyU0
LIBDIR := ../..
LIB := $(LIBDIR)/libavrmodbus.a

CFLAGS += -mmcu=$(MCU) -DF_CPU=$(F_CPU)
CFLAGS += -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS += -I$(LIBDIR)/include -I.

EXAMPLE_MODBUS_DEFS = -DMODBUS_ENABLE_INTERNAL_TICK=1 \
                        -DMODBUS_TICK_TIMER=0 \
                        -DMODBUS_TICK_TIMER0_PRESCALER=64 \
                        -DMODBUS_TICK_FREQUENCY_HZ=1000

CFLAGS += $(MODBUS_DEFS) $(EXAMPLE_MODBUS_DEFS)
LDFLAGS += -Wl,--gc-sections

SOURCES := main.c
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET).hex

$(TARGET).elf: $(OBJECTS) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB):
	$(MAKE) -C $(LIBDIR) MCU=$(MCU) MODBUS_DEFS="$(MODBUS_DEFS) $(EXAMPLE_MODBUS_DEFS)"

clean:
	rm -f $(OBJECTS) $(TARGET).elf $(TARGET).hex

flash: $(TARGET).hex
	avrdude -v -p $(MCU) -c $(FLASHMETHOD) -P $(FLASHDEV) -b $(FLASHBAUD) -D -U flash:w:$(TARGET).hex
