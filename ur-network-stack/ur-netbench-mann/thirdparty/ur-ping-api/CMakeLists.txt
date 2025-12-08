
cmake_minimum_required(VERSION 3.10)
project(ur-ping-api VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library sources
set(PING_LIB_SOURCES
    src/PingAPI.cpp
)

# Create static library
add_library(ping_api STATIC ${PING_LIB_SOURCES})

target_include_directories(ping_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann
)

# Main executable
add_executable(ping_cli src/main.cpp)
target_link_libraries(ping_cli ping_api)

# Installation
install(TARGETS ping_api ping_cli
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES include/PingAPI.hpp
    DESTINATION include
)
