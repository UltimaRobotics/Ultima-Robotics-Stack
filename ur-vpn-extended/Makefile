# Default build directory
BUILD_DIR = build
CMAKE_C_COMPILER = gcc
CMAKE_CXX_COMPILER = g++

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_C_COMPILER=$(CMAKE_C_COMPILER) -DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER) .. && make -j10

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install target
install: all
	cd $(BUILD_DIR) && make install

# Phony targets
.PHONY: all clean install
