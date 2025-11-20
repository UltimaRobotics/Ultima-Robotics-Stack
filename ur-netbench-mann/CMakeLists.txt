cmake_minimum_required(VERSION 3.14)
project(ur-netbench-mann VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add VERBOSE_LOGG option
option(VERBOSE_LOGG "Enable verbose logging" ON)
if(VERBOSE_LOGG)
    add_definitions(-DVERBOSE_LOGG)
    message(STATUS "Verbose logging enabled")
endif()

find_package(Threads REQUIRED)

# Add shared library first
add_subdirectory(ur-netbench-shared)

# Add thirdparty subdirectories
add_subdirectory(thirdparty/dns-lookup-api)

add_subdirectory(thirdparty/traceroute-api)

add_subdirectory(thirdparty/ur-ping-api)

add_subdirectory(thirdparty/ur-iperf-api)

set(BUILD_EXAMPLES OFF CACHE BOOL "Disable threadder examples")
add_subdirectory(thirdparty/ur-threadder-api)

# Add ur-rpc-template
add_subdirectory(thirdparty/ur-rpc-template)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty/traceroute-api/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-threadder-api/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-threadder-api/cpp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-iperf-api/cpp-wrapper/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-iperf-api/src
    ${CMAKE_BINARY_DIR}/thirdparty/ur-iperf-api
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
    ${CMAKE_SOURCE_DIR}/ur-netbench-shared/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-rpc-template
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-rpc-template/extensions
    ${CMAKE_SOURCE_DIR}/thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp
)

# Main executable sources
set(NETBENCH_SOURCES
    src/main.cpp
    src/ServersStatusMonitor.cpp
    src/TestWorkers.cpp
    src/ConfigManager.cpp
    src/FilterUtils.cpp
    src/CLIUtils.cpp
    src/FileWatchdog.cpp
    src/OperationWorker.cpp
    src/RpcClient.cpp
    src/RpcOperationProcessor.cpp
    src/NetbenchOperationHandler.cpp
)

# Main executable
add_executable(netbench-cli ${NETBENCH_SOURCES})

target_link_libraries(netbench-cli
    netbench_shared
    dns_lookup_api
    traceroute_api
    ping_api
    threadmanager_cpp
    threadmanager
    iperf_wrapper
    iperf
    resolv
    ur-rpc-template
    ur-rpc-cpp
    Threads::Threads
)

set_target_properties(netbench-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS netbench-cli
    RUNTIME DESTINATION bin
)