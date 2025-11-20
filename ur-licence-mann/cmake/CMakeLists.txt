
cmake_minimum_required(VERSION 3.10)
project(package_header_generator)

# Set variables that will be substituted in the .h.in files
if(NOT DEFINED PRODUCT_NAME)
    set(PRODUCT_NAME "Ultima AIRLink")
endif()

if(NOT DEFINED PRODUCT_VERSION)
    set(PRODUCT_VERSION "1.0.0")
endif()

if(NOT DEFINED DEVICE_MODEL)
    set(DEVICE_MODEL "AIRLink-Pro-V1")
endif()

if(NOT DEFINED DEVICE_PREFIX)
    set(DEVICE_PREFIX "DEV-UR")
endif()

# Get current timestamp
string(TIMESTAMP CMAKE_TIMESTAMP)

# Configure header files from templates
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/device_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/device_config.h
    @ONLY
)

# Make the configured header available
include_directories(${CMAKE_CURRENT_BINARY_DIR})
