cmake_minimum_required(VERSION 3.10)
project(ur-mavcollector-v2.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/c_library_v2/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-threadder-api/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-threadder-api/cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nlohmann/json)

# UR-RPC Template includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-rpc-template)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-rpc-template/deps/cJSON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-rpc-template/deps/mqtt-client)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-rpc-template/extensions)

# Find required packages
find_package(Threads REQUIRED)

# Add the ur-threadder-api subdirectory (it will handle both C and C++ libraries)
add_subdirectory(thirdparty/ur-threadder-api build/ur-threadder-api)

# Add the ur-rpc-template subdirectory
add_subdirectory(../ur-rpc-template build/ur-rpc-template)

# Source files
set(SOURCES
    src/MavlinkUdpConnection.cpp
    src/MavlinkParser.cpp
    src/BoardIdentifier.cpp
    src/Vehicle.cpp
    src/MavlinkAutopilotVersionInfo.cpp
    src/MavlinkCollectorThread.cpp
    src/rpc_client.cpp
    src/rpc_operation_processor.cpp
    src/main.cpp
)

# Header files
set(HEADERS
    include/MavlinkUdpConnection.h
    include/MavlinkParser.h
    include/BoardIdentifier.h
    include/Vehicle.h
    include/PackageConfig.h
    include/MavlinkCollectorThread.h
    src/rpc_client.hpp
    src/rpc_operation_processor.hpp
)

# Build ur-threadder-api libraries separately first
# Note: These libraries should be built separately before building ur-mavcollector-v2.0

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-threadder-api/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-threadder-api/cpp/include
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
    -ldl -lm
    threadmanager
    threadmanager_cpp
    # UR-RPC Template libraries (built as subdirectory)
    ur-rpc-template
    ur-logger-api-static
    mqtt-client-static
    cJSON
)

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-reorder -Wno-address-of-packed-member -Wno-unused-variable)
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
