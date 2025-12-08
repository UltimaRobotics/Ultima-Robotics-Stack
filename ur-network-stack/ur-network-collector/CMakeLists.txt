cmake_minimum_required(VERSION 3.16)
project(NetworkCollector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the ur-threadder-api subdirectory
add_subdirectory(thirdparty/ur-threadder-api)

# Add the ur-rpc-template subdirectory
add_subdirectory(thirdparty/ur-rpc-template)

include_directories(include thirdparty 
                    thirdparty/ur-threadder-api/include 
                    thirdparty/ur-threadder-api/cpp/include
                    thirdparty/ur-rpc-template
                    thirdparty/ur-rpc-template/deps/cJSON
                    thirdparty/ur-rpc-template/extensions)

add_executable(network_collector
    src/main.cpp
    src/NetworkCollector.cpp
    src/NetworkCollectorThread.cpp
    src/NetworkCollectorThreadGlobals.cpp
    src/RpcClient.cpp
    src/RpcOperationProcessor.cpp
    src/VlanCollector.cpp
    src/NatCollector.cpp
    src/FirewallCollector.cpp
    src/RouteCollector.cpp
    src/BridgeCollector.cpp
)

target_include_directories(network_collector PRIVATE 
    include 
    thirdparty 
    thirdparty/ur-threadder-api/include 
    thirdparty/ur-threadder-api/cpp/include
    thirdparty/ur-rpc-template
    thirdparty/ur-rpc-template/deps/cJSON
    thirdparty/ur-rpc-template/extensions
)

# Link libraries including threadder-api and ur-rpc-template
target_link_libraries(network_collector
    PRIVATE
        threadmanager
        threadmanager_cpp
        ur-rpc-template
        pthread
        stdc++
        m  # Math library for cJSON
)

install(TARGETS network_collector DESTINATION bin)
