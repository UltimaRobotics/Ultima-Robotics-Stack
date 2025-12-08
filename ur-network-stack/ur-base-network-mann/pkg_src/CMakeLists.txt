cmake_minimum_required(VERSION 3.16)
project(OpenWrtNetworkConfig)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(networkconfig STATIC
    src/NetworkConfigAPI.cpp
    src/NetworkMonitor.cpp
    src/ProfileManager.cpp
    src/Utils.cpp
    src/NetworkExtensions.cpp
)

target_link_libraries(networkconfig 
    pthread
)

target_include_directories(networkconfig PUBLIC include)

add_executable(ur-base-network-mann
    src/main.cpp
    src/CLICommands.cpp
)

target_link_libraries(ur-base-network-mann networkconfig)
