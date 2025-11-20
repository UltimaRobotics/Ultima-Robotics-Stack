
cmake_minimum_required(VERSION 3.10)
project(dns_lookup_api)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DNS Lookup API library
add_library(dns_lookup_api STATIC
    src/DNSLookupAPI.cpp
)

target_include_directories(dns_lookup_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann
)

# Main executable
add_executable(dns_cli
    src/main.cpp
)

target_link_libraries(dns_cli
    dns_lookup_api
    resolv
)

# Set output directory
set_target_properties(dns_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
