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
            -mthumb -fno-rtti -fno-exceptions -Os -fno-pic \
            -Wall -Wno-reorder -Wdouble-promotion \
            --param l1-cache-size=8 --param l1-cache-line-size=32 \
            -DARM_MATH_CM7 \
            -MMD -MP -include enosc_plugin_stubs.h
CXXFLAGS += -I. -I$(INCLUDE_PATH) -I$(BUILD_DIR) \
            -I$(ENOSC_DIR) -I$(ENOSC_DIR)/src -I$(ENOSC_DIR)/lib/easiglib
CXXFLAGS += -ffunction-sections -fdata-sections
#CXXFLAGS += -DNT_TEST_STEP

###############################################################################
# Generated Enosc data files
###############################################################################
ENOSC_DATA_CC := $(ENOSC_DIR)/data.cc
ENOSC_DATA_HH := $(ENOSC_DIR)/data.hh        # generated alongside .cc
# DATA_O        := $(BUILD_DIR)/enosc_data.o # No longer building this

###############################################################################
# ---- EXPLICIT list of additional source files you actually need ------------
###############################################################################

# Sources that live *inside* $(ENOSC_DIR)
ENOSC_EXTRA_SRCS := \
    $(ENOSC_DIR)/lib/easiglib/math.cc \
    $(ENOSC_DIR)/lib/easiglib/dsp.cc

# ---- objects for the files inside enosc/ -----------------------------------
ENOSC_OBJ := $(patsubst ./%.cc,$(BUILD_DIR)/%.o,$(filter ./%.cc,$(ENOSC_EXTRA_SRCS)))
ENOSC_OBJ += $(patsubst $(ENOSC_DIR)/%.cc,$(BUILD_DIR)/enosc/%.o,$(filter $(ENOSC_DIR)/%.cc,$(ENOSC_EXTRA_SRCS)))

###############################################################################
# Project-root sources / objects  (e.g. nt_enosc.cpp)
###############################################################################
SRC  := $(filter-out $(ENOSC_DATA_CC),$(wildcard *.cpp *.cc))
OBJ  := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC)))

###############################################################################
# Final artefact
###############################################################################
INTERMEDIATE_OBJECTS := $(OBJ) $(ENOSC_OBJ) # $(DATA_O)
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
# Pattern rules – project-root .cpp/.cc → build/*.o
###############################################################################
$(BUILD_DIR)/%.o: %.cpp $(ENOSC_DATA_HH)
	@echo "Compiling $< → $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Pattern rule – project-root .cc → build/*.o
$(BUILD_DIR)/%.o: %.cc $(ENOSC_DATA_HH)
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
	rm -rf $(BUILD_DIR)

enosc-clean:
	rm -f $(ENOSC_DATA_CC) $(ENOSC_DATA_HH)

check: all
	@echo "Checking for undefined symbols in $(PLUGIN_O)…"
	@arm-none-eabi-nm $(PLUGIN_O) | grep ' U ' || \
	    echo "No undefined symbols found (or grep failed)."
	@echo "(Only _NT_* plus memcpy/memmove/memset should remain undefined.)"

	@echo "Checking .bss footprint…"
		@bss=$$(arm-none-eabi-size -B $(PLUGIN_O) | awk 'NR==2 {print $$3}'); \
			printf ".bss size = %s bytes\n" "$$bss"; \
			if [ "$$bss" -gt 8192 ]; then \
				echo "❌  .bss exceeds 8 KiB limit!  Loader will reject the plug-in."; \
				exit 1; \
			else \
				echo "✅  .bss within limit."; \
			fi

.PHONY: all clean check enosc-clean

###############################################################################
# Auto-generated header dependency includes
###############################################################################
# Only include dependency files ending in .d to avoid erroneously including other files
DEPFILES := $(filter %.d,$(INTERMEDIATE_OBJECTS:.o=.d))
-include $(DEPFILES)
