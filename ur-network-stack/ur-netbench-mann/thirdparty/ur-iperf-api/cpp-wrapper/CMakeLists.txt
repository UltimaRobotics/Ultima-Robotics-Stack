cmake_minimum_required(VERSION 3.10)
project(iperf3_cpp_wrapper VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../nlohmann
)

add_library(iperf_wrapper STATIC
    src/IperfWrapperCore.cpp
    src/IperfWrapperConfig.cpp
    src/IperfWrapperRunner.cpp
    src/IperfWrapperResults.cpp
    src/IperfWrapperCallbacks.cpp
    src/MetricFormatter.cpp
    src/MetricFormatterIntegration.cpp
)

target_include_directories(iperf_wrapper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../nlohmann
)

target_link_libraries(iperf_wrapper PUBLIC
    iperf
)

add_executable(iperf3_cli
    src/main.cpp
)

target_link_libraries(iperf3_cli
    iperf_wrapper
    iperf
)

install(TARGETS iperf3_cli
    RUNTIME DESTINATION bin
)

install(FILES include/IperfWrapper.hpp
    DESTINATION include
)