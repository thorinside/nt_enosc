###############################################################################
# Paths
###############################################################################
ifndef NT_API_PATH
NT_API_PATH := distingNT_API
endif

BUILD_DIR   := build
ENOSC_DIR   := enosc
PLUGIN_DIR  := plugins
INCLUDE_PATH := $(NT_API_PATH)/include

###############################################################################
# Toolchain and flags
###############################################################################
CXX := arm-none-eabi-c++
CXXFLAGS := -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard \
            -mthumb -fno-rtti -fno-exceptions -Os -fno-pic -Wall -Wno-reorder \
            -MMD -MP -include enosc_plugin_stubs.h
CXXFLAGS += -I$(INCLUDE_PATH) -I$(BUILD_DIR) -I. \
            -I$(ENOSC_DIR) -I$(ENOSC_DIR)/src -I$(ENOSC_DIR)/lib/easiglib

###############################################################################
# Generated Enosc data files
###############################################################################
ENOSC_DATA_CC := $(ENOSC_DIR)/data.cc
ENOSC_DATA_HH := $(ENOSC_DIR)/data.hh        # generated alongside .cc
DATA_O        := $(BUILD_DIR)/enosc_data.o

###############################################################################
# ---- EXPLICIT list of additional Enosc sources you actually need ----------
# Add/remove paths here (relative to repo root)
###############################################################################
ENOSC_EXTRA_SRCS := \
    $(ENOSC_DIR)/src/dynamic_data.cc \
	  $(ENOSC_DIR)/lib/easiglib/dsp.cc \
	  $(ENOSC_DIR)/lib/easiglib/math.cc

# Map → objects inside build/enosc/…
ENOSC_EXTRA_OBJS := $(patsubst $(ENOSC_DIR)/%, \
                                $(BUILD_DIR)/$(ENOSC_DIR)/%, \
                                $(ENOSC_EXTRA_SRCS))
ENOSC_EXTRA_OBJS := $(ENOSC_EXTRA_OBJS:.cc=.o)
ENOSC_EXTRA_OBJS := $(ENOSC_EXTRA_OBJS:.cpp=.o)

###############################################################################
# Project-root sources / objects  (e.g. nt_enosc.cpp)
###############################################################################
SRC  := $(filter-out $(ENOSC_DATA_CC),$(wildcard *.cpp))
OBJ  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC))

###############################################################################
# Final artefact
###############################################################################
INTERMEDIATE_OBJECTS := $(OBJ) $(ENOSC_EXTRA_OBJS) $(DATA_O)
PLUGIN_O := $(PLUGIN_DIR)/nt_enosc.o

###############################################################################
# Default target
###############################################################################
all: $(PLUGIN_O)

###############################################################################
# Link: merge everything into one relocatable object
###############################################################################
$(PLUGIN_O): $(INTERMEDIATE_OBJECTS)
	@echo "Linking → $@"
	@mkdir -p $(@D)
	$(CXX) -r -o $@ $^

###############################################################################
# Pattern rules – project-root .cpp → build/*.o
###############################################################################
$(BUILD_DIR)/%.o: %.cpp $(ENOSC_DATA_HH)
	@echo "Compiling $< → $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

###############################################################################
# Pattern rules – explicitly listed enosc sources → build/enosc/**/*.o
###############################################################################
$(BUILD_DIR)/$(ENOSC_DIR)/%.o: $(ENOSC_DIR)/%.cc $(ENOSC_DATA_HH)
	@echo "Compiling $< → $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(ENOSC_DIR)/%.o: $(ENOSC_DIR)/%.cpp $(ENOSC_DATA_HH)
	@echo "Compiling $< → $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

###############################################################################
# Compile rule for generated data object
###############################################################################
$(DATA_O): $(ENOSC_DATA_CC) $(ENOSC_DATA_HH)
	@echo "Compiling $< → $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

###############################################################################
# Rule to **generate** enosc/data.{cc,hh}
###############################################################################
$(ENOSC_DATA_CC) $(ENOSC_DATA_HH): \
        $(ENOSC_DIR)/data/data.py $(ENOSC_DIR)/lib/easiglib/data_compiler.py
	@echo "--- Generating Enosc data files ---"
	(cd $(ENOSC_DIR) && PYTHONPATH=lib/easiglib python3 data/data.py)

###############################################################################
# Convenience targets
###############################################################################
clean:
	rm -f $(PLUGIN_O) $(INTERMEDIATE_OBJECTS) $(DEPFILES)
	rm -rf $(BUILD_DIR)

enosc-clean:
	rm -f $(ENOSC_DATA_CC) $(ENOSC_DATA_HH)

check: all
	@echo "Checking for undefined symbols in $(PLUGIN_O)…"
	@arm-none-eabi-nm $(PLUGIN_O) | grep ' U ' || \
	    echo "No undefined symbols found (or grep failed)."
	@echo "(Only _NT_* plus memcpy/memmove/memset should remain undefined.)"

.PHONY: all clean check enosc-clean

###############################################################################
# Auto-generated header dependency includes
###############################################################################
DEPFILES := $(INTERMEDIATE_OBJECTS:.o=.d)
-include $(DEPFILES)
