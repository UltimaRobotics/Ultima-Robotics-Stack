
cmake_minimum_required(VERSION 3.14)
project(traceroute_api VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann)

# Create library from traceroute.cpp
add_library(traceroute_api STATIC
    src/traceroute.cpp
)

target_include_directories(traceroute_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Optional: Create executable for standalone testing
add_executable(traceroute_app src/main.cpp)
target_link_libraries(traceroute_app traceroute_api)

# Installation
install(TARGETS traceroute_api DESTINATION lib)
install(TARGETS traceroute_app DESTINATION bin)
