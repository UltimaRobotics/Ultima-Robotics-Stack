cmake_minimum_required(VERSION 3.10)
project(ur-vpn-manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# HTTP Server option
option(HTTP_ENABLED "Enable HTTP server support" ON)

# OpenVPN Debug Logging option
option(OPENVPN_DEBUG_LOGGING "Enable OpenVPN UDP/TUN/TAP debug logging" OFF)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ur-openvpn-library/src
    ${CMAKE_SOURCE_DIR}/ur-wg_library/wireguard-wrapper/include
    ${CMAKE_SOURCE_DIR}/ur-vpn-parser
    ${CMAKE_SOURCE_DIR}/ur-vpn-parser/thirdparty
    ${CMAKE_SOURCE_DIR}/ur-threadder-api/cpp/include
    ${CMAKE_SOURCE_DIR}/ur-threadder-api/include
    ${CMAKE_SOURCE_DIR}/ur-rpc-template
    ${CMAKE_SOURCE_DIR}/ur-rpc-template/pkg_src/api/wrappers/cpp
    ${CMAKE_SOURCE_DIR}/ur-rpc-template/deps/mosquitto
    ${CMAKE_SOURCE_DIR}/ur-rpc-template/deps/cjson
)

# Add subdirectories
add_subdirectory(ur-openvpn-library/src)
add_subdirectory(ur-wg_library/wireguard-wrapper)
add_subdirectory(ur-vpn-parser)
add_subdirectory(ur-threadder-api)
add_subdirectory(ur-rpc-template)

# HTTP Server detection
if(HTTP_ENABLED)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(MICROHTTPD libmicrohttpd)
    endif()

    if(MICROHTTPD_FOUND)
        include_directories(${MICROHTTPD_INCLUDE_DIRS})
        message(STATUS "libmicrohttpd found: ${MICROHTTPD_LIBRARIES}")
    else()
        message(WARNING "libmicrohttpd not found. HTTP support will be disabled.")
        set(HTTP_ENABLED OFF)
    endif()
endif()

# VPN Manager modules
set(VPN_MANAGER_MODULES
    src/vpn_instance_manager.cpp
    src/vpn_manager_lifecycle.cpp
    src/vpn_manager_config.cpp
    src/vpn_manager_utils.cpp
    src/vpn_manager_routing.cpp
    src/vpn_manager_events.cpp
    src/vpn_manager_stats.cpp
    src/wireguard_routing_provider.cpp
    src/openvpn_routing_provider.cpp
    src/vpn_rpc_client.cpp
    src/vpn_rpc_operation_processor.cpp
    src/vpn_live_data.cpp
    src/cleanup_tracker.cpp
    src/cleanup_verifier.cpp
    src/cleanup_cron_job.cpp
)

# Executable
add_executable(ur-vpn-manager
    src/main.cpp
    src/http_server.cpp
    ${VPN_MANAGER_MODULES}
    src/vpn_cleanup.cpp
)

# HTTP Server configuration
if(HTTP_ENABLED)
    target_compile_definitions(ur-vpn-manager PRIVATE HTTP_ENABLED)
    message(STATUS "HTTP server support: ENABLED")
else()
    message(STATUS "HTTP server support: DISABLED")
endif()

# Link libraries
target_link_libraries(ur-vpn-manager
    openvpn_cpp_wrapper
    wireguard_cpp_wrapper
    vpn_parser_lib
    threadmanager_cpp
    threadmanager
    ur-rpc-template
    pthread
)

# Link HTTP library if enabled
if(HTTP_ENABLED)
    target_link_libraries(ur-vpn-manager ${MICROHTTPD_LIBRARIES})
endif()

# OpenVPN Debug Logging configuration
if(OPENVPN_DEBUG_LOGGING)
    target_compile_definitions(ur-vpn-manager PRIVATE OPENVPN_DEBUG_LOGGING_ENABLED)
    message(STATUS "OpenVPN debug logging: ENABLED")
else()
    message(STATUS "OpenVPN debug logging: DISABLED")
endif()

# Installation
install(TARGETS ur-vpn-manager DESTINATION bin)
