
cmake_minimum_required(VERSION 3.10)
project(mavdiscovery VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable HTTP support
option(ENABLE_HTTP "Enable HTTP client support" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
find_package(Threads REQUIRED)

# Conditionally find CURL if HTTP is enabled
if(ENABLE_HTTP)
    pkg_check_modules(CURL REQUIRED libcurl)
    add_definitions(-DHTTP_ENABLED)
    message(STATUS "HTTP support enabled")
else()
    message(STATUS "HTTP support disabled")
endif()

# Add the ur-threadder-api subdirectory
add_subdirectory(ur-threadder-api)
message(STATUS "LIBS = ${THREADDER_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS}")

# Add the ur-rpc-template subdirectory
add_subdirectory(../ur-rpc-template ur-rpc-template)
message(STATUS "RPC TEMPLATE LIBS = ${RPC_TEMPLATE_LIBS}")
message(STATUS "RPC TEMPLATE INCLUDE_DIRS = ${RPC_TEMPLATE_INCLUDE_DIRS}")

# Add the ur-mavdiscovery-shared subdirectory
add_subdirectory(ur-mavdiscovery-shared)
message(STATUS "MAVLINK SHARED LIBS = ${UR_MAVDISCOVERY_SHARED_LIBS}")
message(STATUS "MAVLINK SHARED INCLUDE_DIRS = ${UR_MAVDISCOVERY_SHARED_INCLUDE_DIRS}")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/c_library_v2
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nholmann
    ${UDEV_INCLUDE_DIRS}
    ${THREADDER_INCLUDE_DIRS}
    ${RPC_TEMPLATE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/ur-mavdiscovery-shared/include
)

# Conditionally include CURL directories
if(ENABLE_HTTP)
    include_directories(${CURL_INCLUDE_DIRS})
endif()


# Source files
set(SOURCES
    src/main.cpp
    src/Logger.cpp
    src/ConfigLoader.cpp
    src/DeviceStateDB.cpp
    src/SerialPort.cpp
    src/MAVLinkParser.cpp
    src/DeviceVerifier.cpp
    src/DeviceMonitor.cpp
    src/CallbackDispatcher.cpp
    src/DeviceManager.cpp
    src/RpcClient.cpp
    src/RpcOperationProcessor.cpp
    src/DeviceDiscoveryCronJob.cpp
    src/USBDeviceTracker.cpp
)

# Conditionally add HTTP client source
if(ENABLE_HTTP)
    list(APPEND SOURCES src/HTTPClient.cpp)
endif()

# Create executable
add_executable(ur-mavdiscovery ${SOURCES})

# Link libraries
target_link_libraries(ur-mavdiscovery
    ${UDEV_LIBRARIES}
    threadmanager
    threadmanager_cpp
    ur-rpc-template
    ur-rpc-dcpp
    ur-mavdiscovery-shared
    Threads::Threads
    m
    ssl
    crypto
)

# Conditionally link CURL
if(ENABLE_HTTP)
    target_link_libraries(ur-mavdiscovery ${CURL_LIBRARIES})
endif()

# Compiler flags
target_compile_options(ur-mavdiscovery PRIVATE
    -Wall -Wextra
    -Wno-address-of-packed-member
    -O2
    -Wno-unused-but-set-variable
)