cmake_minimum_required(VERSION 3.10)
project(FlightCollector LANGUAGES CXX C)

# Set C++ standard requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find system libraries
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Build options
option(BUILD_CLI "Build CLI application" ON)
option(BUILD_TESTS "Build tests" OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JSONCPP_INCLUDE_DIRS}
)

# Source files
set(COLLECTOR_SOURCES
    src/FlightCollector.cpp
    src/ConfigParser.cpp
    src/DataStructures.cpp
    src/JsonFormatter.cpp
)

# Header files
set(COLLECTOR_HEADERS
    include/FlightCollector.h
    include/ConfigParser.h
    include/DataStructures.h
    include/JsonFormatter.h
)

# Create static library
add_library(ur-mavcollector-mavsdk STATIC ${COLLECTOR_SOURCES} ${COLLECTOR_HEADERS})

# Add MAVSDK include paths to the target to avoid conflicts with system installation
target_include_directories(ur-mavcollector-mavsdk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/core/include/mavsdk
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/telemetry/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/param/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/info/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/battery/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/mavlink
    ${THREADDER_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(ur-mavcollector-mavsdk PRIVATE
    mavsdk
    ${THREADDER_LIBS}
    ${JSONCPP_LIBRARIES}
    pthread
)

# Compiler flags
target_compile_options(ur-mavcollector-mavsdk PRIVATE
    -Wall -Wextra -g
)

# Build CLI application
if(BUILD_CLI)
    add_executable(ur-mavcollector-mavsdk-cli
        src/main.cpp
    )
    
    target_link_libraries(ur-mavcollector-mavsdk-cli PRIVATE
        ur-mavcollector-mavsdk
        mavsdk
        ${THREADDER_LIBS}
        ${JSONCPP_LIBRARIES}
        pthread
    )
    
    # Add MAVSDK include paths to CLI executable
    target_include_directories(ur-mavcollector-mavsdk-cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/core/include/mavsdk
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/telemetry/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/param/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/info/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../mavsdk/plugins/battery/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/mavlink
        ${THREADDER_INCLUDE_DIRS}
    )
    
    target_compile_options(ur-mavcollector-mavsdk-cli PRIVATE
        -Wall -Wextra -g
    )
endif()

# Installation
install(TARGETS ur-mavcollector-mavsdk
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${COLLECTOR_HEADERS}
    DESTINATION include/ur-mavcollector-mavsdk
)

# Install CLI applications
if(BUILD_CLI)
    install(TARGETS ur-mavcollector-mavsdk-cli #ur-mavcollector-mavsdk-threaded
        RUNTIME DESTINATION bin
    )
endif()

install(DIRECTORY include/ DESTINATION include)
