cmake_minimum_required(VERSION 3.10)
project(ur-qmi-ident)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-incomplete-type -Wno-incomplete-declared -D_DEBUG_LOG")

pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Add dependency subdirectories to build them automatically
add_subdirectory(../ur-threadder-api threadder-api-build)
add_subdirectory(../ur-rpc-template rpc-template-build)



add_executable(ur-qmi-ident
    src/main.cpp
    src/qmi_scanner.cpp
    src/qmi_dev_scan_agent.cpp
    src/qmi_device_registry.cpp
    src/rpc_client.cpp
    src/rpc_operation_processor.cpp
)

target_include_directories(ur-qmi-ident PRIVATE 
    ${UDEV_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    src
    ../ur-threadder-api/include
    ../ur-rpc-template
    ../ur-rpc-template/extensions
    threadder-api-build/include
    rpc-template-build
    rpc-template-build/extensions
    rpc-template-build/deps/cJSON
    rpc-template-build/deps/mqtt-client/src
    rpc-template-build/deps/ur-logger-api
)

target_link_libraries(ur-qmi-ident 
    ${UDEV_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    pthread
    ur-rpc-template
    threadmanager
    mqtt-client-static
    cJSON
    ur-logger-api-static
    Threads::Threads
    m
    ssl
    crypto
)

target_compile_options(ur-qmi-ident PRIVATE 
    ${UDEV_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
)
