cmake_minimum_required(VERSION 3.14)
project(netbench_shared)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(netbench_shared STATIC
    src/DNSResultSerializer.cpp
    src/TracerouteResultSerializer.cpp
    src/PingResultSerializer.cpp
    src/IperfResultSerializer.cpp
    src/ServerStatusSerializer.cpp
    src/ServerStatusProgressSerializer.cpp
)

target_include_directories(netbench_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)