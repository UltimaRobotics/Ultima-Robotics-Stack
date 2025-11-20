cmake_minimum_required(VERSION 3.10)
project(ur-qmi-ident)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-incomplete-type -Wno-incomplete-declared -D_DEBUG_LOG")

pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Add subdirectories directly (no custom target needed)
add_subdirectory(thirdparty/ur-rpc-template)
add_subdirectory(thirdparty/ur-threadder-api)

message(STATUS "LIBS = ${THREADDER_LIBS} ${RPC_TEMPLATE_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS} ${RPC_TEMPLATE_INCLUDE_DIRS}")

add_executable(ur-qmi-ident
    src/main.cpp
    src/qmi_scanner.cpp
    src/qmi_dev_scan_agent.cpp
    src/qmi_device_registry.cpp
    shared/user-level.cpp
)

target_include_directories(ur-qmi-ident PRIVATE 
    ${UDEV_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${THREADDER_INCLUDE_DIRS}
    ${RPC_TEMPLATE_INCLUDE_DIRS}
    src
    shared
)

target_link_libraries(ur-qmi-ident 
    ${UDEV_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${THREADDER_LIBS}
    ${RPC_TEMPLATE_LIBS}
    pthread
)

target_compile_options(ur-qmi-ident PRIVATE 
    ${UDEV_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
)
