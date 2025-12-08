cmake_minimum_required(VERSION 3.16)
project(ur-wireless-tools VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Add third-party directories
add_subdirectory(thirdparty/ur-rpc-template)
add_subdirectory(thirdparty/ur-threadder-api)

# Add C++ wrapper include directory
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp/include)

# Create core library
add_library(urwt-core
    src/models/wifi_interface.cpp
    src/models/network_info.cpp
    src/models/scan_result.cpp
    src/models/connection_test_result.cpp
    src/utils/process_executor.cpp
    src/utils/json_formatter.cpp
    src/parsers/iw_scan_parser.cpp
    src/strategies/forked_scan_strategy.cpp
    src/strategies/threaded_scan_strategy.cpp
    src/strategies/pipe_scan_strategy.cpp
    src/managers/interface_detector.cpp
    src/managers/interface_manager.cpp
    src/managers/connection_tester.cpp
    src/api.cpp
    src/rpc/rpc_types.cpp
    src/rpc/operation_tracker.cpp
    src/rpc/request_dispatcher.cpp
    src/rpc/rpc_service.cpp
    src/rpc/handlers/list_interfaces_handler.cpp
    src/rpc/handlers/scan_networks_handler.cpp
    src/rpc/handlers/get_interface_info_handler.cpp
    src/rpc/handlers/test_connection_handler.cpp
    src/rpc/handlers/get_status_handler.cpp
    src/rpc/handlers/shutdown_handler.cpp
    src/rpc/handlers/save_network_handler.cpp
    src/rpc/handlers/remove_network_handler.cpp
    src/rpc/handlers/list_saved_networks_handler.cpp
    src/rpc/handlers/set_mode_handler.cpp
    src/rpc/handlers/enable_wifi_handler.cpp
    src/rpc/handlers/disable_wifi_handler.cpp
    src/rpc/handlers/set_automation_handler.cpp
    src/rpc/handlers/get_wireless_config_handler.cpp
    src/rpc/handlers/update_wireless_config_handler.cpp
    src/rpc/handlers/get_system_state_handler.cpp
    src/config/config_manager.cpp
    src/config/wireless_config_manager.cpp
    src/config/configuration_persister.cpp
    src/config/startup_configurator.cpp
    src/state/wireless_state.cpp
    src/state/system_state_analyzer.cpp
    src/mode/mode_controller.cpp
    src/mode/sta_mode_manager.cpp
    src/mode/ap_mode_manager.cpp
    src/network/saved_network_manager.cpp
    src/network/connection_manager.cpp
    src/events/event_types.cpp
    src/events/event_publisher.cpp
    src/monitor/network_monitor.cpp
    src/monitor/scan_result_cache.cpp
)

target_include_directories(urwt-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${THREADDER_INCLUDE_DIRS}
        ${RPC_TEMPLATE_INCLUDE_DIRS}
)

target_link_libraries(urwt-core
    PUBLIC
        Threads::Threads
        threadmanager_cpp
        ur-rpc-cpp
        ur-rpc-dcpp
)

# RPC service executable
add_executable(ur-wireless-tools
    src/main.cpp
)

target_link_libraries(ur-wireless-tools
    PRIVATE
        urwt-core
)

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()