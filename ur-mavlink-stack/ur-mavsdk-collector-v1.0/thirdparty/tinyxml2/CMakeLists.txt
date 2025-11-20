cmake_minimum_required(VERSION 3.10)
project(tinyxml2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create library target
add_library(tinyxml2 STATIC
    tinyxml2.cpp
)

# Include directories
target_include_directories(tinyxml2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Public headers (for IDE organization)
target_sources(tinyxml2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2.h
)

