AVR_PREFIX = avr
CC := $(AVR_PREFIX)-gcc
AR := $(AVR_PREFIX)-ar
OBJCOPY := $(AVR_PREFIX)-objcopy
OBJDUMP := $(AVR_PREFIX)-objdump

MCU ?= atmega328p
F_CPU ?= 16000000UL

MODBUS_DEFS ?=

CFLAGS += -mmcu=$(MCU) -DF_CPU=$(F_CPU)
CFLAGS += -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS += -Iinclude
CFLAGS += $(MODBUS_DEFS)

LDFLAGS += -Wl,--gc-sections

OBJDIR := build
SRCDIR := src
SOURCES := $(SRCDIR)/modbus_config.c \
           $(SRCDIR)/modbus_core.c \
           $(SRCDIR)/modbus_hw.c \
           $(SRCDIR)/modbus_registers.c
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

LIB := libavrmodbus.a

.PHONY: all clean flash example

all: $(LIB)

$(LIB): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(LIB)

example:
	@$(MAKE) -C examples/basic all

flash:
	@echo "Use the example makefile to flash specific hardware."
