cmake_minimum_required(VERSION 3.16)
project(controlled-log)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(CONTROLLED_LOG_SOURCES
    src/controlled_log.cpp
)

# Header files
set(CONTROLLED_LOG_HEADERS
    include/controlled_log.h
)

# Create static library
add_library(controlled-log STATIC ${CONTROLLED_LOG_SOURCES} ${CONTROLLED_LOG_HEADERS})

# Include directories
target_include_directories(controlled-log PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann/json
)

# Link libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
if(NOT NLOHMANN_JSON_FOUND)
    # Try find_package as fallback
    find_package(nlohmann_json QUIET)
    if(nlohmann_json_FOUND)
        message(STATUS "Found nlohmann_json via find_package")
        set(NLOHMANN_JSON_INCLUDE_DIRS ${nlohmann_json_INCLUDE_DIRS})
        set(NLOHMANN_JSON_LIBRARIES ${nlohmann_json_LIBRARIES})
        set(NLOHMANN_JSON_FOUND TRUE)
    else()
        # Use header-only fallback
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann/json.hpp")
            message(STATUS "Using header-only nlohmann_json fallback")
            set(NLOHMANN_JSON_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann")
        else()
            message(FATAL_ERROR "nlohmann_json is required but not found")
        endif()
    endif()
endif()

target_include_directories(controlled-log PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})

# Set compiler flags
target_compile_features(controlled-log PRIVATE cxx_std_17)

# Installation
install(TARGETS controlled-log
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${CONTROLLED_LOG_HEADERS}
    DESTINATION include
)
