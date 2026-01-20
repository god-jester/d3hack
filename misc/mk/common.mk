#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#
# CONFIG_JSON is the filename of the NPDM config file (.json), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.json
#     - config.json
#---------------------------------------------------------------------------------
export TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
ROOT_SOURCE	:=	$(TOPDIR)/source
MODULES		:=	$(shell find $(ROOT_SOURCE) -mindepth 1 -maxdepth 1 -type d)
SOURCES		:=	$(foreach module,$(MODULES),$(shell find $(module) -type d))
SOURCES		:= 	$(foreach source,$(SOURCES),$(source:$(TOPDIR)/%=%)/)

# Exclude Dear ImGui example sources.
SOURCES		:=	$(filter-out source/third_party/imgui/examples/ source/third_party/imgui/examples/%,$(SOURCES))

# Exclude Dear ImGui built-in backends and misc/tools.
SOURCES		:=	$(filter-out source/third_party/imgui/backends/ source/third_party/imgui/backends/%,$(SOURCES))
SOURCES		:=	$(filter-out source/third_party/imgui/docs/ source/third_party/imgui/docs/%,$(SOURCES))
SOURCES		:=	$(filter-out source/third_party/imgui/misc/ source/third_party/imgui/misc/%,$(SOURCES))

DATA		:=	data
INCLUDES	:=	include

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -fvisibility=hidden

CFLAGS	:=	-g -Wall -Werror -Ofast \
			-ffunction-sections \
			-Wno-format-zero-length \
			-fdata-sections \
			$(ARCH) \
			$(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -D__RTLD_6XX__ -DIMGUI_USER_CONFIG=\"imgui_backend/nvn_imgui_config.h\" -isystem $(ROOT_SOURCE)/third_party/imgui

CFLAGS	+= $(EXL_CFLAGS) -isystem" $(DEVKITPRO)/libnx/include" -I$(ROOT_SOURCE) $(addprefix -isystem, $(MODULES))

CXXFLAGS	:= $(CFLAGS) $(EXL_CXXFLAGS) -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-threadsafe-statics -std=gnu++23

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	:=  -specs=$(SPECS_PATH)/$(SPECS_NAME) -g $(ARCH) -Wl,-Map,$(notdir $*.map) -nostartfiles

LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

ifneq (,$(findstring source/third_party/imgui/examples/,$(SOURCES)))
# Drop Dear ImGui example entrypoints.
CPPFILES	:=	$(filter-out main.cpp,$(CPPFILES))
endif

# Drop unused Dear ImGui misc helpers.
CPPFILES	:=	$(filter-out imgui_demo.cpp imgui_freetype.cpp imgui_stdlib.cpp binary_to_compressed_c.cpp,$(CPPFILES))

# Exclude official Dear ImGui backends (we use custom NVN backend).
CPPFILES	:=	$(filter-out imgui_impl_%.cpp,$(CPPFILES))

# Re-add our custom NVN backend after filtering.
CPPFILES	:=	$(filter-out imgui_impl_nvn.cpp,$(CPPFILES))
CPPFILES	+=	imgui_impl_nvn.cpp

# Exclude Dear ImGui example programs.
CPPFILES	:=	$(filter-out example_%.cpp,$(CPPFILES))
CPPFILES	:=	$(filter-out uSynergy.c,$(CPPFILES))

# Ensure the project entrypoint is always compiled.
CPPFILES	:=	$(filter-out main.cpp,$(CPPFILES))
CPPFILES	+=	main.cpp
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
# Only .bin files have build rules (see %.bin.o below).
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD) -I$(ROOT_SOURCE)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(MK_PATH)/common.mk
	@$(SHELL) $(SCRIPTS_PATH)/post-build.sh

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nso $(TARGET).npdm $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all 

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

all	:	$(OUTPUT).nso

$(OUTPUT).nso	:	$(OUTPUT).elf $(OUTPUT).npdm

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
