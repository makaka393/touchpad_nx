#---------------------------------------------------------------------------------
# TouchPad overlay for Tesla / Ultrahand (nx-ovlloader).
#   DEVKITPRO=<path> make        -> touchpad.ovl
#   make -C tests test           -> host unit tests for core/
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

APP_TITLE   := TouchPad
APP_AUTHOR  := makaka393
APP_VERSION := 0.8.0

TARGET      := touchpad
BUILD       := build
SOURCES     := source core
NO_ICON     := 1

ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS   := -g -Wall -Wextra -Wno-missing-field-initializers -O2 -ffunction-sections -fdata-sections $(ARCH) \
            -D__SWITCH__ -DAPP_VERSION="\"$(APP_VERSION)\""
CFLAGS   += $(INCLUDE) -isystem $(TOPDIR)/third_party
CXXFLAGS := $(CFLAGS) -std=gnu++20 -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(notdir $*.map)
LIBS     := -lnx
LIBDIRS  := $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

export LD       := $(CXX)
export OFILES   := $(CPPFILES:.cpp=.o)
export INCLUDE  := $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@mkdir -p out/switch/.overlays out/config/touchpad/examples
	@cp $(TARGET).ovl out/switch/.overlays/$(TARGET).ovl
	@cp packs/*.ini out/config/touchpad/examples/

clean:
	@rm -fr $(BUILD) out $(TARGET).ovl $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else

.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).ovl

$(OUTPUT).ovl: $(OUTPUT).elf $(OUTPUT).nacp
	@elf2nro $< $@ $(NROFLAGS)
	@echo "built ... $(notdir $@)"

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
