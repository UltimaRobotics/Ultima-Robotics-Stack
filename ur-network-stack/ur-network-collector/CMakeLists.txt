cmake_minimum_required(VERSION 3.16)
project(NetworkCollector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include thirdparty)

add_executable(network_collector
    src/main.cpp
    src/NetworkCollector.cpp
    src/VlanCollector.cpp
    src/NatCollector.cpp
    src/FirewallCollector.cpp
    src/RouteCollector.cpp
    src/BridgeCollector.cpp
)

target_include_directories(network_collector PRIVATE include thirdparty)

install(TARGETS network_collector DESTINATION bin)
