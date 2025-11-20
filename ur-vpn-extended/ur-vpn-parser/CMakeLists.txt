
cmake_minimum_required(VERSION 3.10)
project(vpn-parser VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

# VPN Parser library source files
set(PARSER_SOURCES
    vpn_parser.cpp
)

set(PARSER_HEADERS
    vpn_parser.hpp
    thirdparty/nlohmann/json.hpp
)

# Create parser library
add_library(vpn_parser_lib STATIC ${PARSER_SOURCES} ${PARSER_HEADERS})

# VPN Parser CLI executable
add_executable(vpn-parser main.cpp)
target_link_libraries(vpn-parser vpn_parser_lib)

# Install target
install(TARGETS vpn-parser vpn_parser_lib DESTINATION bin)

# Enable testing
enable_testing()

# Custom target for running tests
add_custom_target(test_run
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_example.sh
    DEPENDS vpn-parser
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running VPN parser tests"
)
