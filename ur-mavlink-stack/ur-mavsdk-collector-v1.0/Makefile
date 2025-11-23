# Ultimate Robotics MAVSDK Collector Makefile
# This makefile provides a simple interface to build the entire project

# Default build directory
BUILD_DIR = build

# Default target
all: $(BUILD_DIR)
	@echo "Building Ultimate Robotics MAVSDK Collector..."
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CLI=ON
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "Build completed successfully!"
	@echo "Executable: $(BUILD_DIR)/collector/ur-mavcollector-mavsdk-cli"
	@echo "Libraries: $(BUILD_DIR)/collector/libur-mavcollector-mavsdk.a"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Debug build
debug: $(BUILD_DIR)
	@echo "Building Ultimate Robotics MAVSDK Collector (Debug)..."
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_CLI=ON
	@cd $(BUILD_DIR) && make -j$$(nproc)

# Release build
release: $(BUILD_DIR)
	@echo "Building Ultimate Robotics MAVSDK Collector (Release)..."
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CLI=ON
	@cd $(BUILD_DIR) && make -j$$(nproc)

# Phony targets
.PHONY: all clean debug release help install uninstall

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@echo "Clean completed!"

# Install target (optional)
install: all
	@echo "Installing Ultimate Robotics MAVSDK Collector..."
	@cd $(BUILD_DIR) && make install
	@echo "Install completed!"

# Uninstall target (optional)
uninstall:
	@echo "Uninstalling Ultimate Robotics MAVSDK Collector..."
	@cd $(BUILD_DIR) && make uninstall 2>/dev/null || echo "No uninstall target available"
	@echo "Uninstall completed!"

# Help target
help:
	@echo "Ultimate Robotics MAVSDK Collector Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the entire project (libraries and CLI)"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release version"
	@echo "  clean     - Remove all build artifacts"
	@echo "  install   - Install the built binaries"
	@echo "  uninstall - Remove installed binaries"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make          - Build everything (same as 'make all')"
	@echo "  make clean    - Clean build directory"
	@echo "  make debug    - Build debug version"
	@echo "  make release  - Build optimized release version"
	@echo "  make install  - Build and install"
	@echo ""
	@echo "Build outputs:"
	@echo "  CLI Executable: $(BUILD_DIR)/collector/ur-mavcollector-mavsdk-cli"
	@echo "  Static Library: $(BUILD_DIR)/collector/libur-mavcollector-mavsdk.a"
	@echo "  RPC Libraries: $(BUILD_DIR)/ur_rpc_template_build/lib*.a"

