cmake_minimum_required(VERSION 3.10)
project(ur-mavrouter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable HTTP server build
option(_BUILD_HTTP "Build with HTTP server support" OFF)

add_definitions(-DPACKAGE="ur-mavrouter")
add_definitions(-Wno-address-of-packed-member)
add_definitions(-Wno-return-type)
add_definitions(-Wno-unused-result)
add_definitions(-O2)
add_definitions(-g)

# Add the ur-threadder-api subdirectory
add_subdirectory(modules/ur-threadder-api)
message(STATUS "LIBS = ${THREADDER_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS}")

# Add the ur-rpc-template subdirectory (like ur-mavdiscovery)
add_subdirectory(../../ur-rpc-template ur-rpc-template)
message(STATUS "RPC TEMPLATE LIBS = ${RPC_TEMPLATE_LIBS}")
message(STATUS "RPC TEMPLATE INCLUDE_DIRS = ${RPC_TEMPLATE_INCLUDE_DIRS}")

# Add the ur-mavdiscovery-shared subdirectory
add_subdirectory(ur-mavdiscovery-shared)
message(STATUS "MAVDISCOVERY SHARED LIBS = ${UR_MAVDISCOVERY_SHARED_LIBS}")
message(STATUS "MAVDISCOVERY SHARED INCLUDE_DIRS = ${UR_MAVDISCOVERY_SHARED_INCLUDE_DIRS}")

# Conditionally add HTTP module
if(_BUILD_HTTP)
    message(STATUS "HTTP server support enabled")
    add_definitions(-D_BUILD_HTTP)

    # Add the http-module subdirectory
    add_subdirectory(modules/http-module)
    message(STATUS "HTTP_MODULE_LIBS = ${HTTP_MODULE_LIBS}")
    message(STATUS "HTTP_MODULE_INCLUDE_DIRS = ${HTTP_MODULE_INCLUDE_DIRS}")

    # Find libmicrohttpd for main executable
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MHD REQUIRED libmicrohttpd)
else()
    message(STATUS "HTTP server support disabled")
    set(HTTP_MODULE_LIBS "")
    set(HTTP_MODULE_INCLUDE_DIRS "")
    set(MHD_INCLUDE_DIRS "")
endif()

include_directories(
    .
    modules/mavlink_c_library_v2/ardupilotmega
    ${THREADDER_INCLUDE_DIRS}
    ${RPC_TEMPLATE_INCLUDE_DIRS}
    ${HTTP_MODULE_INCLUDE_DIRS}
    ${MHD_INCLUDE_DIRS}
    modules/nholmann
    src
    src/rpc-mechanisms
    src/mavlink-extensions
    ur-mavdiscovery-shared/include
)

set(SRC_FILES
    src/autolog.cpp
    src/binlog.cpp
    src/dedup.cpp
    src/endpoint.cpp
    src/endpoint_stats.cpp
    src/git_version.cpp
    src/logendpoint.cpp
    src/main.cpp
    src/mainloop.cpp
    src/pollable.cpp
    src/timeout.cpp
    src/ulog.cpp
    src/tlog.cpp
    src/common/conf_file.cpp
    src/common/json_config.cpp
    src/common/log.cpp
    src/common/util.cpp
    src/common/xtermios.cpp
    src/rpc-mechanisms/RpcClient.cpp
    src/rpc-mechanisms/RpcOperations.cpp
    src/rpc-mechanisms/RpcClientInterfaceNew.cpp
    src/rpc-mechanisms/RpcControllerNew.cpp
    src/mavlink-extensions/ExtensionManager.cpp
)

configure_file(src/version.h.in src/git_version.h)
add_executable(${PROJECT_NAME} ${SRC_FILES} src/git_version.h)
target_link_libraries(${PROJECT_NAME} rt m pthread ${THREADDER_LIBS} ${RPC_TEMPLATE_LIBS} ${HTTP_MODULE_LIBS} ur-mavdiscovery-shared)
