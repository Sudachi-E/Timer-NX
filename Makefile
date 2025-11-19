#---------------------------------------------------------------------------------
# Switch Timer Makefile
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# Setup devkitPro
#---------------------------------------------------------------------------------
ifndef DEVKITPRO
    $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

#---------------------------------------------------------------------------------
# Target and sources
#---------------------------------------------------------------------------------
TARGET      := SwitchTimer
BUILD       := build
SOURCES     := source
INCLUDES    := include

#---------------------------------------------------------------------------------
# Tools
#---------------------------------------------------------------------------------
PREFIX      := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC          := $(PREFIX)gcc
CXX         := $(PREFIX)g++
AS          := $(PREFIX)as
AR          := $(PREFIX)ar
OBJCOPY     := $(PREFIX)objcopy
STRIP       := $(PREFIX)strip
NM          := $(PREFIX)nm
ELF2NRO     := $(DEVKITPRO)/tools/bin/elf2nro

#---------------------------------------------------------------------------------
# File lists
#---------------------------------------------------------------------------------
CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES    := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
OFILES      := $(addprefix $(BUILD)/, $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.s=.o))

# Create build directories
$(shell mkdir -p $(BUILD) $(addprefix $(BUILD)/,$(SOURCES)))

#---------------------------------------------------------------------------------
# Compiler and linker flags
#---------------------------------------------------------------------------------
ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS      := -g -Wall -O2 -ffunction-sections $(ARCH) -D__SWITCH__
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(TARGET).map

# Include paths
INCLUDES    := -I$(CURDIR)/$(INCLUDES) \
               -I$(DEVKITPRO)/libnx/include \
               -I$(DEVKITPRO)/portlibs/switch/include

# Library paths
LIBPATHS    := -L$(DEVKITPRO)/libnx/lib \
               -L$(DEVKITPRO)/portlibs/switch/lib

# Libraries
LIBS        := -lnx -lm

# Default icon
APP_ICON    := $(CURDIR)/icon.jpg

#---------------------------------------------------------------------------------
# Build targets
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).nro

$(TARGET).nro: $(TARGET).elf
	@echo "[NRO] $@"
	@$(ELF2NRO) $< $@ --icon=$(APP_ICON)

$(TARGET).elf: $(OFILES)
	@echo "[LD]  $@"
	@$(CC) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@echo "[CC]  $<"
	@$(CC) -MMD -MP -MF $(@:.o=.d) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@echo "[CXX] $<"
	@$(CXX) -MMD -MP -MF $(@:.o=.d) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.s
	@echo "[AS]  $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

clean:
	@echo "[CLEAN]"
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nso $(TARGET).map

# Include dependency files
-include $(wildcard $(BUILD)/*.d)

#---------------------------------------------------------------------------------
