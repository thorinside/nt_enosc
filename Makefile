ifndef NT_API_PATH
	NT_API_PATH := distingNT_API
endif

INCLUDE_PATH := $(NT_API_PATH)/include

BUILD_DIR := build

inputs := $(wildcard *cpp)
inputs := $(filter-out $(BUILD_DIR)/data.cc, $(inputs))
outputs := $(patsubst %.cpp,plugins/%.o,$(inputs))

outputs += $(BUILD_DIR)/data.o

ENOSC_DATA_DIR := enosc
ENOSC_DATA_CC := $(ENOSC_DATA_DIR)/data.cc
ENOSC_DATA_HH := $(ENOSC_DATA_DIR)/data.hh

all: $(ENOSC_DATA_CC) $(ENOSC_DATA_HH) $(outputs)

clean:
	rm -f $(outputs)
	rm -rf $(BUILD_DIR)

check: all
	@echo "Checking for undefined symbols in $(outputs)..."
	@arm-none-eabi-nm $(outputs) | grep ' U ' || echo "No undefined symbols found (or grep failed to find any)."
	@echo "Note: If symbols are listed above, they are undefined in the plugin and expected to be provided by the host."

.PHONY: all clean check

$(ENOSC_DATA_CC) $(ENOSC_DATA_HH): $(ENOSC_DATA_DIR)/data/data.py $(ENOSC_DATA_DIR)/lib/easiglib/data_compiler.py
	(cd $(ENOSC_DATA_DIR) && PYTHONPATH=lib/easiglib python3 data/data.py)

$(BUILD_DIR)/data.cc $(BUILD_DIR)/data.hh: scripts/generate_data.py scripts/data_compiler.py
	mkdir -p $(BUILD_DIR)
	PYTHONPATH=scripts python3 scripts/generate_data.py --output-dir=$(BUILD_DIR)

$(BUILD_DIR)/data.o: $(BUILD_DIR)/data.cc $(BUILD_DIR)/data.hh
	arm-none-eabi-c++ -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-rtti -fno-exceptions -Os -fno-pic -Wall -Wno-reorder -I$(INCLUDE_PATH) -I$(BUILD_DIR) -I. -I$(ENOSC_DATA_DIR) -c -o $@ $<

plugins/%.o: %.cpp $(BUILD_DIR)/data.cc $(BUILD_DIR)/data.hh
	mkdir -p $(@D)
	arm-none-eabi-c++ -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-rtti -fno-exceptions -Os -fno-pic -Wall -Wno-reorder -I$(INCLUDE_PATH) -I$(BUILD_DIR) -I. -I$(ENOSC_DATA_DIR) -c -o $@ $<
