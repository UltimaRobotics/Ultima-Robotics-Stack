cmake_minimum_required(VERSION 3.10)
project(QMIConnectionManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Find required packages
find_package(PkgConfig REQUIRED)

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
add_executable(qmi_connection_manager ${SOURCES} ${HEADERS})

# Create mutex test executable
add_executable(test_mutex_implementation
    test_mutex_implementation.cpp
)

# Link libraries
target_link_libraries(qmi_connection_manager 
    ${JSONCPP_LIBRARIES}
    pthread
)

# Link libraries for test
target_link_libraries(test_mutex_implementation
    pthread
)

# Set compiler flags
if(JSONCPP_CFLAGS_OTHER)
    target_compile_options(qmi_connection_manager PRIVATE ${JSONCPP_CFLAGS_OTHER})
endif()

# Install target
install(TARGETS qmi_connection_manager DESTINATION bin)