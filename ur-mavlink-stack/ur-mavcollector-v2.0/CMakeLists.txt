cmake_minimum_required(VERSION 3.10)
project(ur-mavcollector-v2.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/c_library_v2/common)

# Find required packages
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/MavlinkUdpConnection.cpp
    src/MavlinkParser.cpp
    src/BoardIdentifier.cpp
    src/Vehicle.cpp
    src/MavlinkAutopilotVersionInfo.cpp
    src/main.cpp
)

# Header files
set(HEADERS
    include/MavlinkUdpConnection.h
    include/MavlinkParser.h
    include/BoardIdentifier.h
    include/Vehicle.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
)

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
