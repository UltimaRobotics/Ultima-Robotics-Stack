cmake_minimum_required(VERSION 3.10)
project(QMIConnectionManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-unused-variable -Wno-unused-result")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-incomplete-type -Wno-incomplete-declared -D_DEBUG_LOG")

# Find nlohmann JSON
find_package(nlohmann_json REQUIRED)

# Add dependency subdirectories to build them automatically
add_subdirectory(../ur-threadder-api threadder-api-build)
add_subdirectory(../ur-rpc-template ur-rpc-template-build)

# Source files
set(SOURCES
    src/connection/connection_manager.cpp
    src/connection/qmi_session_handler.cpp
    src/connection/connection_state_machine.cpp
    src/connection/interface_controller.cpp
    src/monitoring/connectivity_monitor.cpp
    src/monitoring/failure_detector.cpp
    src/recovery/recovery_engine.cpp
    src/monitoring/metrics_reporter.cpp
    src/connection/connection_registry.cpp
    src/recovery/smart_routing.cpp
    src/utils/command_logger.cpp
    src/utils/timeout_config.cpp
    src/connection/multi_connection_support.cpp
    src/monitoring/ip_monitor.cpp
    src/rpc/rpc_client.cpp
    src/rpc/rpc_operation_processor.cpp
    src/core/main.cpp
)

# Header files
set(HEADERS
    src/connection/connection_manager.h
    src/utils/command_logger.h
    src/utils/timeout_config.h
    src/recovery/smart_routing.h
    src/connection/qmi_session_handler.h
    src/connection/connection_state_machine.h
    src/connection/interface_controller.h
    src/monitoring/connectivity_monitor.h
    src/monitoring/failure_detector.h
    src/recovery/recovery_engine.h
    src/monitoring/metrics_reporter.h
    src/rpc/rpc_client.h
    src/rpc/rpc_operation_processor.h
)

# Create main executable
add_executable(ur-qmi-launcher ${SOURCES} ${HEADERS})

target_include_directories(ur-qmi-launcher PRIVATE 
    ../ur-threadder-api/include
    ../ur-threadder-api/cpp/include
    threadder-api-build/include
    threadder-api-build/cpp/include
    ../ur-rpc-template
    ../ur-rpc-template/deps/cJSON
    ../ur-rpc-template/deps/mqtt-client/src
    ../ur-rpc-template/deps/ur-logger-api
    ../ur-rpc-template/extensions
    ../ur-rpc-template/pkg_src/api/wrappers/cpp
    ur-rpc-template-build
    src
    src/core
    src/connection
    src/monitoring
    src/recovery
    src/rpc
    src/utils
    include
    include/core
)

# Link libraries
target_link_libraries(ur-qmi-launcher 
    nlohmann_json::nlohmann_json
    pthread
    threadmanager
    threadmanager_cpp
    ur-rpc-template
    ur-rpc-dcpp
    Threads::Threads
    m
)

# Install target
install(TARGETS ur-qmi-launcher DESTINATION bin)