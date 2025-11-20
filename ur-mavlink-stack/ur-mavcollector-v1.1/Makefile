# Default build directory
BUILD_DIR = build

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make -j$(nproc)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Phony targets
.PHONY: all clean http-enabled http-disabled install

all: http-enabled

http-enabled:
	@echo "Building with HTTP support enabled..."
	mkdir -p build && cd build && cmake -DENABLE_HTTP=ON .. && make

http-disabled:
	@echo "Building without HTTP support..."
	mkdir -p build && cd build && cmake -DENABLE_HTTP=OFF .. && make

clean:
	rm -rf build

# Install target
install: all
	cd $(BUILD_DIR) && make install

