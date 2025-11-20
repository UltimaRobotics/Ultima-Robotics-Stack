# Default build directory
BUILD_DIR = build

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make -j6

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Phony targets
.PHONY: all clean 

clean:
	rm -rf build

