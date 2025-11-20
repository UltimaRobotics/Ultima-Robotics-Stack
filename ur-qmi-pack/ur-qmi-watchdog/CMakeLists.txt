
cmake_minimum_required(VERSION 3.10)
project(ur-qmi-watchdog)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter")


# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
if(JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
endif()
# Add subdirectories directly (no custom target needed)
add_subdirectory(thirdparty/ur-rpc-template)
add_subdirectory(thirdparty/ur-threadder-api)

message(STATUS "LIBS = ${THREADDER_LIBS} ${RPC_TEMPLATE_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS} ${RPC_TEMPLATE_INCLUDE_DIRS}")

# Add executable
add_executable(ur-qmi-watchdog
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qmi_watchdog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/user-level.cpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)


# Link libraries
target_link_libraries(ur-qmi-watchdog
    ${JSONCPP_LIBRARIES}
    pthread
)

target_include_directories(ur-qmi-watchdog PRIVATE 
    ${UDEV_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${THREADDER_INCLUDE_DIRS}
    ${RPC_TEMPLATE_INCLUDE_DIRS}
    src
    shared
)

target_link_libraries(ur-qmi-watchdog 
    ${UDEV_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${THREADDER_LIBS}
    ${RPC_TEMPLATE_LIBS}
    pthread
)

# Add compiler flags for jsoncpp
if(JSONCPP_FOUND)
    target_compile_options(ur-qmi-watchdog PRIVATE ${JSONCPP_CFLAGS_OTHER})
    target_include_directories(ur-qmi-watchdog PRIVATE ${JSONCPP_INCLUDE_DIRS})
endif()

# Installation
install(TARGETS ur-qmi-watchdog DESTINATION bin)

# Print configuration info
message(STATUS "QMI Watchdog Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")

if(JSONCPP_FOUND)
    message(STATUS "  JsonCpp: Found")
    message(STATUS "    Version: ${JSONCPP_VERSION}")
    message(STATUS "    Libraries: ${JSONCPP_LIBRARIES}")
else()
    message(STATUS "  JsonCpp: Not found")
endif()
