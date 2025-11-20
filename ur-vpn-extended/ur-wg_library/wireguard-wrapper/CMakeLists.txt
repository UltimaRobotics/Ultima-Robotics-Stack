
cmake_minimum_required(VERSION 3.10)
project(wireguard-wrapper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build ur-wg-provider subdirectory first
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ur-wg-provider ${CMAKE_CURRENT_BINARY_DIR}/ur-wg-provider)

# Ensure wg-provider and wg-tunnel use the correct config.h by prioritizing the include directory
target_include_directories(wg-provider BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ur-wg-provider/include)
target_include_directories(wg-tunnel BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ur-wg-provider/include)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-threadder-api/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-threadder-api/cpp/include
)

# C Bridge library
add_library(wireguard_c_bridge STATIC
    src/wireguard_c_bridge.c
)

target_link_libraries(wireguard_c_bridge
    wg-provider
    threadmanager
    pthread
)

# C++ Wrapper library
add_library(wireguard_cpp_wrapper STATIC
    src/wireguard_wrapper.cpp
)

target_link_libraries(wireguard_cpp_wrapper
    wireguard_c_bridge
    threadmanager_cpp
    pthread
)

# CLI Application
add_executable(wireguard_cpp_cli
    src/main.cpp
)

target_link_libraries(wireguard_cpp_cli
    wireguard_cpp_wrapper
    wg-provider
    threadmanager_cpp
    pthread
)

# Installation
install(TARGETS wireguard_c_bridge wireguard_cpp_wrapper wireguard_cpp_cli
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    include/wireguard_c_bridge.h
    include/wireguard_wrapper.hpp
    DESTINATION include
)
