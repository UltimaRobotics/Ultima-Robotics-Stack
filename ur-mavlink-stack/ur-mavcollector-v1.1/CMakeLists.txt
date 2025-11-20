cmake_minimum_required(VERSION 3.10)
project(ur-mavcollector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-Wno-address-of-packed-member)
add_definitions(-Wno-return-type)
add_definitions(-Wno-unused-result)
add_definitions(-O2)
add_definitions(-g)

# Option to enable HTTP support
option(ENABLE_HTTP "Enable HTTP client support" OFF)

# Add include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mavlink_c_library_v2
    ${CMAKE_CURRENT_SOURCE_DIR}/mavlink_c_library_v2/common
    ${CMAKE_CURRENT_SOURCE_DIR}/nholmann
)

# Add subdirectory for threadder API
add_subdirectory(ur-threadder-api)

# Conditionally add HTTP module
if(ENABLE_HTTP)
    add_subdirectory(http-module)
    add_definitions(-DHTTP_ENABLED)
    include_directories(${HTTP_CLIENT_INCLUDE_DIRS})
    message(STATUS "HTTP support enabled")
else()
    message(STATUS "HTTP support disabled")
endif()

# MAVLink Collector executable
add_executable(mavlink_collector
    src/UdpHandler.cpp
    src/MAVLinkCollector.cpp
    src/collector_main.cpp
)

# Link libraries
target_link_libraries(mavlink_collector ${THREADDER_LIBS} pthread)

# Conditionally link HTTP client
if(ENABLE_HTTP)
    target_link_libraries(mavlink_collector ${HTTP_CLIENT_LIBS})
endif()