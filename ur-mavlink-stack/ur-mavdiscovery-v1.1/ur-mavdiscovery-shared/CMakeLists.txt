cmake_minimum_required(VERSION 3.10)
project(ur-mavdiscovery-shared VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/nholmann
    ${CMAKE_CURRENT_SOURCE_DIR}/../ur-rpc-template/deps/nlohmann/include
)

# Collect source files
set(SOURCES
    src/MavlinkDeviceStructs.cpp
    src/MavlinkEventSerializer.cpp
)

# Collect header files
set(HEADERS
    include/MavlinkDeviceStructs.hpp
    include/MavlinkEventSerializer.hpp
)

# Create static library
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${HEADERS}"
)

# Include directories for targets using this library
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link with required libraries
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
)
