.DEFAULT_GOAL := all

# This project lives at <devkit>/workspace/exercises/block_breaker.
# Derive the bundled environment so make also works outside start-uiap.command.
UIAP_DEVKIT_ROOT ?= $(abspath $(CURDIR)/../../..)
UIAP_PLATFORM ?= mac
UIAP_WORKSPACE ?= $(UIAP_DEVKIT_ROOT)/workspace
UIAP_RUNTIME ?= $(UIAP_DEVKIT_ROOT)/runtime/mac
UIAP_TOOLCHAIN_BIN ?= $(UIAP_RUNTIME)/toolchain/bin
export PATH := $(UIAP_DEVKIT_ROOT)/scripts/bin:$(UIAP_RUNTIME)/bin:$(UIAP_RUNTIME)/python/bin:$(UIAP_TOOLCHAIN_BIN):/usr/bin:/bin:/usr/sbin:/sbin

ifeq ($(wildcard $(UIAP_WORKSPACE)/deps/ch32fun/ch32fun/ch32fun.mk),)
$(error UIAP Devkit was not found at $(UIAP_DEVKIT_ROOT))
endif
ifeq ($(wildcard $(UIAP_TOOLCHAIN_BIN)/riscv-none-elf-gcc),)
$(error Bundled RISC-V toolchain was not found at $(UIAP_TOOLCHAIN_BIN))
endif

CH32FUN_ROOT := $(UIAP_WORKSPACE)/deps/ch32fun
CH32FUN := $(CH32FUN_ROOT)/ch32fun
MINICHLINK := $(UIAP_RUNTIME)/bin

TARGET := oled_rotary_test
TARGET_MCU := CH32V003
MCU_PACKAGE := 1
PREFIX := $(UIAP_TOOLCHAIN_BIN)/riscv-none-elf
FLASH_COMMAND = "$(MINICHLINK)/minichlink" -c 0x1209b803 -w "$<" $(WRITE_SECTION) -b

include $(CH32FUN)/ch32fun.mk

all: build
flash: cv_flash
clean: cv_clean
size: build
	$(PREFIX)-size $(TARGET).elf
doctor:
	@doctor
report:
	@report
help:
	@echo make
	@echo make size
	@echo make flash
	@echo make clean
	@echo make doctor
	@echo make report

.PHONY: all flash clean size doctor report help
