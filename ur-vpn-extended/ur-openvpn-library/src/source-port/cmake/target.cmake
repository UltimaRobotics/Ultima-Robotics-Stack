# Combine all sources
set(ALL_SOURCES ${OPENVPN_CORE_SOURCES} ${COMPAT_SOURCES} ${ROUTING_API_SOURCES})

# Create static library
add_library(openvpn_static STATIC ${ALL_SOURCES})
set_target_properties(openvpn_static PROPERTIES OUTPUT_NAME openvpn)
target_include_directories(openvpn_static PRIVATE
    ${CMAKE_SOURCE_DIR}/lib-src
)

# Create shared library
add_library(openvpn_shared SHARED ${ALL_SOURCES})
set_target_properties(openvpn_shared PROPERTIES OUTPUT_NAME openvpn)
target_include_directories(openvpn_shared PRIVATE
    ${CMAKE_SOURCE_DIR}/lib-src
)

# Set compile definitions
target_compile_definitions(openvpn_static PRIVATE
    HAVE_CONFIG_H
    -DPLUGIN_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib/openvpn/plugins"
    -DDEFAULT_DNS_UPDOWN="${CMAKE_INSTALL_PREFIX}/libexec/openvpn/dns-updown"
    $<$<PLATFORM_ID:Windows>:_WIN32>
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_GNU_SOURCE>
)

target_compile_definitions(openvpn_shared PRIVATE
    HAVE_CONFIG_H
    -DPLUGIN_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib/openvpn/plugins"
    -DDEFAULT_DNS_UPDOWN="${CMAKE_INSTALL_PREFIX}/libexec/openvpn/dns-updown"
    $<$<PLATFORM_ID:Windows>:_WIN32>
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_GNU_SOURCE>
)

# Link libraries for static target
target_link_libraries(openvpn_static
    ${OPENSSL_LIBRARIES}
    ${LZO_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${LIBNL_LIBRARIES}
    $<$<NOT:$<PLATFORM_ID:Windows>>:dl>
    $<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
    $<$<PLATFORM_ID:Windows>:iphlpapi>
    $<$<PLATFORM_ID:Windows>:winmm>
    $<$<PLATFORM_ID:Windows>:fwpuclnt>
)

# Link libraries for shared target
target_link_libraries(openvpn_shared
    ${OPENSSL_LIBRARIES}
    ${LZO_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${LIBNL_LIBRARIES}
    $<$<NOT:$<PLATFORM_ID:Windows>>:dl>
    $<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
    $<$<PLATFORM_ID:Windows>:iphlpapi>
    $<$<PLATFORM_ID:Windows>:winmm>
    $<$<PLATFORM_ID:Windows>:fwpuclnt>
)

# Include directories for both targets
target_include_directories(openvpn_static PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/install/include
)

target_include_directories(openvpn_shared PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/install/include
)

# Create openvpn executable
add_executable(openvpn_exe lib-src/openvpn.c)
set_target_properties(openvpn_exe PROPERTIES OUTPUT_NAME openvpn-bin)

target_link_libraries(openvpn_exe 
    openvpn_static
)

target_compile_definitions(openvpn_exe PRIVATE
    HAVE_CONFIG_H
    OPENVPN_BUILD_EXECUTABLE
    $<$<PLATFORM_ID:Windows>:_WIN32>
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_GNU_SOURCE>
)

