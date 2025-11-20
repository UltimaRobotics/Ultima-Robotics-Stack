
cmake_minimum_required(VERSION 3.18)

project(ur-licence-shared VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# nlohmann::json configuration
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty
)

# Source files
set(SHARED_SOURCES
    src/operation_types.cpp
)

set(SHARED_HEADERS
    include/operation_types.hpp
)

# Create static library
add_library(ur-licence-shared STATIC ${SHARED_SOURCES} ${SHARED_HEADERS})

# Set include directories for the library
target_include_directories(ur-licence-shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler features
target_compile_features(ur-licence-shared PUBLIC cxx_std_20)

# Compiler flags
target_compile_options(ur-licence-shared PRIVATE -Wall -Wextra)

# Installation rules
install(TARGETS ur-licence-shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Export the library
set_target_properties(ur-licence-shared PROPERTIES
    OUTPUT_NAME "ur-licence-shared"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "ur-licence-shared library configured")
