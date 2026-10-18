SHELL := cmd.exe
.SHELLFLAGS := /C
.RECIPEPREFIX := >

.DEFAULT_GOAL := all

TARGET := stm32_cooperative_scheduler
BUILD := build
LINKER_SCRIPT := STM32F407VGTX_FLASH.ld

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

PROGRAMMER ?= STM32_Programmer_CLI.exe
STLINK_SN ?= 0671FF495170535067222342

CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CPPFLAGS := -IInc

CFLAGS := $(CPU_FLAGS) -std=c11 -Og -g3
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -MMD -MP

ASFLAGS := $(CPU_FLAGS) -g3 -x assembler-with-cpp

LDFLAGS := $(CPU_FLAGS) -T$(LINKER_SCRIPT) -nostartfiles
LDFLAGS += --specs=nano.specs --specs=nosys.specs
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map

LDLIBS := -Wl,--start-group -lc -lgcc -Wl,--end-group

C_SOURCES := $(wildcard Src/*.c)
C_OBJECTS := $(patsubst Src/%.c,$(BUILD)/%.o,$(C_SOURCES))
STARTUP_OBJECT := $(BUILD)/startup_stm32f407vgtx.o

OBJECTS := $(C_OBJECTS) $(STARTUP_OBJECT)
DEPS := $(C_OBJECTS:.o=.d)

ELF := $(BUILD)/$(TARGET).elf
BIN := $(BUILD)/$(TARGET).bin
HEX := $(BUILD)/$(TARGET).hex

.PHONY: all clean flash size

all: $(ELF) $(BIN) $(HEX)

$(BUILD):
>if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BUILD)/%.o: Src/%.c Makefile | $(BUILD)
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STARTUP_OBJECT): Startup/startup_stm32f407vgtx.s Makefile | $(BUILD)
>$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJECTS) $(LINKER_SCRIPT) Makefile
>$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
>$(SIZE) $@

$(BIN): $(ELF)
>$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
>$(OBJCOPY) -O ihex $< $@

size: $(ELF)
>$(SIZE) $(ELF)

flash: $(ELF)
>"$(PROGRAMMER)" -c port=SWD mode=UR sn=$(STLINK_SN) -w "$(ELF)" -v -rst

clean:
>if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"

-include $(DEPS)
