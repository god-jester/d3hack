# Define paths.
PWD := $(shell pwd)
MISC_PATH := $(PWD)/misc
MK_PATH := $(MISC_PATH)/mk
SCRIPTS_PATH := $(MISC_PATH)/scripts
NPDM_JSON_PATH := $(MISC_PATH)/npdm-json

# To configure exlaunch, edit config.mk.
include $(PWD)/config.mk

# Define common variables.
NAME := $(shell basename $(PWD))
OUT := $(PWD)/deploy
SD_OUT := atmosphere/contents/$(PROGRAM_ID)

# Set load kind specific variables.
ifeq ($(LOAD_KIND), Module)
    LOAD_KIND_ENUM := 2
    BINARY_NAME := subsdk9 # TODO: support subsdkX?
else ifeq ($(LOAD_KIND), AsRtld)
    LOAD_KIND_ENUM := 1
    BINARY_NAME := rtld
else
    $(error LOAD_KIND is invalid, please check config.mk)
endif

# Export all of our variables to sub-processes.
export

CMAKE ?= cmake
BUILD_DIR ?= build

.PHONY: clean all

all:
	@$(CMAKE) --toolchain=cmake/toolchain.cmake -S . -B $(BUILD_DIR)
	@$(MAKE) -C $(BUILD_DIR)
	@cp $(BUILD_DIR)/$(NAME) $(PWD)/$(NAME).elf
	@cp $(BUILD_DIR)/$(NAME).nso $(PWD)/$(NAME).nso
	@cp $(BUILD_DIR)/$(NAME).npdm $(PWD)/$(NAME).npdm
	@$(SHELL) $(SCRIPTS_PATH)/post-build.sh

clean:
	@echo clean ...
	@rm -fr $(BUILD_DIR) $(NAME).nso $(NAME).npdm $(NAME).elf
	@$(SHELL) $(SCRIPTS_PATH)/pre-build.sh

include $(MK_PATH)/deploy.mk
include $(MK_PATH)/npdm.mk
