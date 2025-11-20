cmake_minimum_required(VERSION 3.10)
project(ur-rpc-integration VERSION 1.0 LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
find_package(Threads REQUIRED)

# Find cJSON (use bundled if not found)
find_package(cJSON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api/wrappers
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api/wrappers/cpp
    ${MOSQUITTO_INCLUDE_DIRS}
    ${cJSON_INCLUDE_DIRS}
    ${THREADDER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/mavlink_c_library_v2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/mavlink_c_library_v2/ardupilotmega
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/mavlink_c_library_v2/common
)

# Compiler definitions
add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-UR_RPC_TEMPLATE_COMPILATION)

# Core UR-RPC Template source files
set(UR_RPC_CORE_SOURCES
    ur-rpc-template.c
    direct_template.c
    logger.c
    rpc_client_wrapper.cpp
    rpc_operation_processor.cpp
    pkg_src/api/wrappers/cpp/extensions/direct_template.cpp
    pkg_src/api/wrappers/cpp/gateway/gateway.cpp
)

# Add cJSON source if using bundled version
if(NOT cJSON_LIBRARIES)
    # Use bundled cJSON from ur-threadder-api
    list(APPEND UR_RPC_CORE_SOURCES ../ur-threadder-api/thirdparty/cJSON.c)
    include_directories(../ur-threadder-api/thirdparty)
endif()

# Create the UR-RPC integration library
add_library(ur-rpc-integration STATIC ${UR_RPC_CORE_SOURCES})

# Link libraries
target_link_libraries(ur-rpc-integration 
    PRIVATE 
    Threads::Threads 
    ${MOSQUITTO_LIBRARIES} 
    ${cJSON_LIBRARIES}
    ${THREADDER_LIBS}
    m
)

# Create C++ wrapper library
add_library(ur-rpc-integration-cpp STATIC
    pkg_src/api/wrappers/cpp/extensions/direct_template.cpp
    pkg_src/api/wrappers/cpp/gateway/gateway.cpp
)

target_link_libraries(ur-rpc-integration-cpp 
    PRIVATE 
    ur-rpc-integration
    Threads::Threads
    ${MOSQUITTO_LIBRARIES}
    ${cJSON_LIBRARIES}
    ${THREADDER_LIBS}
)

# Installation rules
install(TARGETS ur-rpc-integration ur-rpc-integration-cpp DESTINATION lib)
install(FILES 
    ur-rpc-template.h
    pkg_src/api/wrappers/cpp/ur-rpc-template.hpp
    pkg_src/api/wrappers/cpp/extensions/direct_template.hpp
    pkg_src/api/wrappers/cpp/gateway/gateway.hpp
    DESTINATION include/ur-rpc-integration
)

# Export variables for parent CMakeLists.txt
set(UR_RPC_INTEGRATION_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api/wrappers
    ${CMAKE_CURRENT_SOURCE_DIR}/pkg_src/api/wrappers/cpp
    ${MOSQUITTO_INCLUDE_DIRS}
    ${cJSON_INCLUDE_DIRS}
    CACHE INTERNAL "UR-RPC Integration include directories"
)

set(UR_RPC_INTEGRATION_LIBS
    ur-rpc-integration
    ur-rpc-integration-cpp
    CACHE INTERNAL "UR-RPC Integration libraries"
)
