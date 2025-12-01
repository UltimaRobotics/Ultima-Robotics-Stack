cmake_minimum_required(VERSION 3.10)
project(QMIConnectionManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-incomplete-type -Wno-incomplete-declared -D_DEBUG_LOG")

# Try to find jsoncpp
pkg_check_modules(JSONCPP jsoncpp)

if(NOT JSONCPP_FOUND)
    # Fallback to find_package
    find_package(PkgConfig REQUIRED)
    find_library(JSONCPP_LIBRARIES NAMES jsoncpp)
    if(NOT JSONCPP_LIBRARIES)
        message(STATUS "jsoncpp not found via pkg-config, using system includes")
        set(JSONCPP_LIBRARIES "jsoncpp")
    endif()
endif()

# Include directories
if(JSONCPP_INCLUDE_DIRS)
    include_directories(${JSONCPP_INCLUDE_DIRS})
endif()

# Add dependency subdirectories to build them automatically
add_subdirectory(../ur-threadder-api threadder-api-build)

# Source files
set(SOURCES
    connection_manager.cpp
    qmi_session_handler.cpp
    connection_state_machine.cpp
    interface_controller.cpp
    connectivity_monitor.cpp
    failure_detector.cpp
    recovery_engine.cpp
    metrics_reporter.cpp
    connection_registry.cpp
    smart_routing.cpp
    command_logger.cpp
    timeout_config.cpp
    multi_connection_support.cpp
    ip_monitor.cpp
    main.cpp
)

# Header files
set(HEADERS
    connection_manager.h
    command_logger.h
    timeout_config.h
    smart_routing.h
    qmi_session_handler.h
    connection_state_machine.h
    interface_controller.h
    connectivity_monitor.h
    failure_detector.h
    recovery_engine.h
    metrics_reporter.h
)

# Create main executable
add_executable(ur-qmi-launcher ${SOURCES} ${HEADERS})

target_include_directories(ur-qmi-launcher PRIVATE 
    ${JSONCPP_INCLUDE_DIRS}
    ../ur-threadder-api/include
    ../ur-threadder-api/cpp/include
    threadder-api-build/include
    threadder-api-build/cpp/include
)

# Link libraries
target_link_libraries(ur-qmi-launcher 
    ${JSONCPP_LIBRARIES}
    pthread
    threadmanager
    threadmanager_cpp
    Threads::Threads
    m
)

# Set compiler flags
if(JSONCPP_CFLAGS_OTHER)
    target_compile_options(ur-qmi-launcher PRIVATE ${JSONCPP_CFLAGS_OTHER})
endif()

# Install target
install(TARGETS ur-qmi-launcher DESTINATION bin)