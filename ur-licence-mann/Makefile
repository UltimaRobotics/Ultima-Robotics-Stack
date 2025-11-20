# Default build directory
BUILD_DIR = build

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DPRODUCT_NAME="Ultima AIRLink Pro" -DPRODUCT_VERSION="2.0.0" -DDEVICE_MODEL="AIRLink-Pro-V2" .. && make -j12

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Phony targets
.PHONY: all clean

clean:
	rm -rf build

