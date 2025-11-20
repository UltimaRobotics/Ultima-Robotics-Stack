
cmake_minimum_required(VERSION 3.10)
project(openvpn_cpp_wrapper VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Force Linux platform detection
set(WIN32 OFF CACHE BOOL "Disable Windows platform" FORCE)
set(WIN32_PLATFORM OFF CACHE BOOL "Disable Windows platform" FORCE)
set(UNIX ON CACHE BOOL "Enable Unix platform" FORCE)

# Set the CMAKE_MODULE_PATH for source-port cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/source-port/cmake")

# Include the main OpenVPN library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source-port)

# Get OpenSSL package info
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)

# Include directories from source-port
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source-port/lib-src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/source-port
    ${OPENSSL_INCLUDE_DIRS}
)

# C Bridge - Pure C interface to lib-src
add_library(openvpn_c_bridge STATIC
    openvpn_c_bridge.c
    openvpn_c_bridge.h
)

target_compile_definitions(openvpn_c_bridge PRIVATE
    HAVE_CONFIG_H
    -DPLUGIN_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib/openvpn/plugins"
    -DDEFAULT_DNS_UPDOWN="${CMAKE_INSTALL_PREFIX}/libexec/openvpn/dns-updown"
    $<$<PLATFORM_ID:Windows>:_WIN32>
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_GNU_SOURCE>
)

target_link_libraries(openvpn_c_bridge
    openvpn_static
    ${OPENSSL_LIBRARIES}
    pthread
)

target_include_directories(openvpn_c_bridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source-port/lib-src
    ${CMAKE_CURRENT_SOURCE_DIR}/source-port
    ${CMAKE_CURRENT_SOURCE_DIR}/source-port/install/include
)

# C++ Wrapper - uses C bridge to access lib-src
add_library(openvpn_cpp_wrapper STATIC
    openvpn_wrapper.cpp
    openvpn_wrapper.hpp
)

target_link_libraries(openvpn_cpp_wrapper
    openvpn_c_bridge
    ${OPENSSL_LIBRARIES}
    pthread
)

target_include_directories(openvpn_cpp_wrapper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

# Main executable using the C++ wrapper
add_executable(openvpn_cpp_demo main.cpp)

target_link_libraries(openvpn_cpp_demo
    openvpn_cpp_wrapper
    openvpn_static
    ${OPENSSL_LIBRARIES}
    pthread
)

# Installation
install(TARGETS openvpn_static DESTINATION lib)
install(TARGETS openvpn_shared DESTINATION lib)
install(TARGETS openvpn_exe DESTINATION bin RENAME openvpn)
install(TARGETS openvpn_cpp_wrapper DESTINATION lib)
install(TARGETS openvpn_cpp_demo DESTINATION bin)
