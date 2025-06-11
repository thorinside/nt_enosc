
ifndef NT_API_PATH
	NT_API_PATH := distingNT_API
endif

INCLUDE_PATH := $(NT_API_PATH)/include

inputs := $(wildcard *cpp)
outputs := $(patsubst %.cpp,plugins/%.o,$(inputs))

all: $(outputs)

clean:
	rm -f $(outputs)

check: all
	@echo "Checking for undefined symbols in $(outputs)..."
	@arm-none-eabi-nm $(outputs) | grep ' U ' || echo "No undefined symbols found (or grep failed to find any)."
	@echo "Note: If symbols are listed above, they are undefined in the plugin and expected to be provided by the host."

.PHONY: all clean check

plugins/%.o: %.cpp
	mkdir -p $(@D)
	arm-none-eabi-c++ -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-rtti -fno-exceptions -Os -fno-pic -Wall -Wno-reorder -I$(INCLUDE_PATH) -c -o $@ $^
