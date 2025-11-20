cmake_minimum_required(VERSION 3.10)
project(mavlink)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create interface library (header-only)
add_library(mavlink INTERFACE)

# Add include directories
target_include_directories(mavlink INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mavlink>
    $<INSTALL_INTERFACE:include>
)

# Add all header files to the target
file(GLOB_RECURSE MAVLINK_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/mavlink/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/mavlink/*.hpp"
)

# Add headers to target sources (for IDE organization)
target_sources(mavlink INTERFACE ${MAVLINK_HEADERS})

# Installation rules
install(TARGETS mavlink EXPORT mavlinkTargets)
install(DIRECTORY mavlink/
    DESTINATION include/mavlink
    FILES_MATCHING PATTERN "*.h"
)


