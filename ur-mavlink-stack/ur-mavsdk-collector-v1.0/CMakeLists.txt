cmake_minimum_required(VERSION 3.10)
project(MAVSDK_Project LANGUAGES CXX C)

# Set C++ standard requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-inline")

# Build size optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Balanced optimization (size/speed)
    set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    # Maximum size optimization (use -Os if -Oz not available)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-Oz" COMPILER_SUPPORTS_OZ)
    if(COMPILER_SUPPORTS_OZ)
        set(CMAKE_C_FLAGS_MINSIZEREL "-Oz -DNDEBUG")
        set(CMAKE_CXX_FLAGS_MINSIZEREL "-Oz -DNDEBUG")
    else()
        set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")
        set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
    endif()
elseif(NOT CMAKE_BUILD_TYPE)
    # Default to size-optimized release build if no type specified
    set(CMAKE_BUILD_TYPE "MinSizeRel")
    set(CMAKE_C_FLAGS "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS "-Os -DNDEBUG")
endif()

# Common flags for all build types
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-ffunction-sections>
)

# Linker flags for size optimization
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

# Find required packages
find_package(PkgConfig REQUIRED)

# Find system libraries
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(ZLIB REQUIRED zlib)

# Build third-party dependencies (only static ones)
add_subdirectory(thirdparty)

# Force static build for ThreadManager before building it
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
add_subdirectory(thirdparty/ur-threadder-api ur-threadder-api_build)

# Add ur-rpc-template as subdirectory
add_subdirectory(../ur-rpc-template ur_rpc_template_build)

# Then build MAVSDK core and plugins
add_subdirectory(mavsdk)

# Define MAVSDK include path variables for use in other subdirectories
# These are based on the paths defined in mavsdk/CMakeLists.txt
set(MAVSDK_INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/mavsdk
    ${CMAKE_CURRENT_SOURCE_DIR}/mavsdk/core
    ${CMAKE_CURRENT_SOURCE_DIR}/mavsdk/core/include/mavsdk
    ${CMAKE_CURRENT_SOURCE_DIR}/mavsdk/plugins
)

# Add all plugin directories to include paths
file(GLOB_RECURSE PLUGIN_DIRS 
    LIST_DIRECTORIES true
    "${CMAKE_CURRENT_SOURCE_DIR}/mavsdk/plugins/*"
)

# Filter to only directories (exclude files)
set(PLUGIN_DIRS_ONLY "")
foreach(item ${PLUGIN_DIRS})
    if(IS_DIRECTORY ${item})
        list(APPEND PLUGIN_DIRS_ONLY ${item})
    endif()
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES PLUGIN_DIRS_ONLY)

# Add plugin directories to MAVSDK include paths
list(APPEND MAVSDK_INCLUDE_PATHS ${PLUGIN_DIRS_ONLY})

# Define third-party include paths
set(MAVSDK_THIRDPARTY_INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mavlink
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/tinyxml2
    ${JSONCPP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

# Build the flight collector application
add_subdirectory(collector)