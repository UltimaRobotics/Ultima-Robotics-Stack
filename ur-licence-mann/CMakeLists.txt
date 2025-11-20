cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Force GCC compiler for C++20 support
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

# Project definition
project(ur-licence-mann
    VERSION 1.0.0
    DESCRIPTION "Advanced License Management System with Encryption and Hardware Fingerprinting"
    LANGUAGES CXX
)
set(CMAKE_BUILD_TYPE "Release")

# Add the ur-threadder-api subdirectory
add_subdirectory(thirdparty/ur-threadder-api)
add_subdirectory(thirdparty/ur-rpc-template)

message(STATUS "LIBS = ${THREADDER_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS}")

add_compile_options(-Wno-deprecated-declarations -Wno-unused-parameter )
add_compile_options(-Wno-unused-variable -Wno-reorder )

# Product configuration (can be overridden via -D flags)
if(NOT DEFINED PRODUCT_NAME)
    set(PRODUCT_NAME "Ultima AIRLink")
endif()

if(NOT DEFINED PRODUCT_VERSION)
    set(PRODUCT_VERSION "1.0.0")
endif()

if(NOT DEFINED DEVICE_MODEL)
    set(DEVICE_MODEL "AIRLink-Pro-V1")
endif()

# Configure device_config.h from template
string(TIMESTAMP CMAKE_TIMESTAMP)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/device_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/device_config.h
    @ONLY
)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static linking configuration - disabled for compatibility with system OpenSSL
option(BUILD_STATIC "Build statically linked executable" OFF)

if(BUILD_STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    # Don't force all libraries to be static - allow dynamic linking for system libraries like OpenSSL
    # set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    # set(BUILD_SHARED_LIBS OFF)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Include directories
include_directories(
    src/
    ${CMAKE_CURRENT_BINARY_DIR}
    thirdparty
    thirdparty/CLI11
    thirdparty/licensecxx/modules/crypto/include
    thirdparty/licensecxx/modules/identifiers/include
    thirdparty/licensecxx/modules/ident_utils/include
    thirdparty/licensecxx/modules/lcxx/include
    thirdparty/licensecxx/modules/license/include
    shared-library/include
    thirdparty/ur-threadder-api/include
)

# Find OpenSSL using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found via pkg-config")
    message(STATUS "OpenSSL include dirs: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
    include_directories(${OPENSSL_INCLUDE_DIRS})
    link_directories(${OPENSSL_LIBRARY_DIRS})
endif()

# Find Threads (optional in static builds)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# nlohmann::json configuration
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

# Add licensecxx subdirectory (disable samples and tests)
set(LCXX_GENERATE_KEYS OFF CACHE BOOL "Disable key generation in licensecxx" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable testing in licensecxx" FORCE)
add_subdirectory(thirdparty/licensecxx EXCLUDE_FROM_ALL)

# Add shared library
add_subdirectory(shared-library)

# Source files
set(SOURCES
    src/main.cpp
    src/config_manager.cpp
    src/crypto_utils.cpp
    src/feature_manager.cpp
    src/file_watchdog.cpp
    src/hardware_fingerprint.cpp
    src/init_manager.cpp
    src/license_manager.cpp
    src/operation_handler.cpp
    src/startup_manager.cpp
    src/rpc_client.cpp
    src/rpc_operation_processor.cpp
)

set(HEADERS
    src/license_manager.hpp
    src/license_types.hpp
    src/crypto_utils.hpp
    src/hardware_fingerprint.hpp
    src/config_manager.hpp
    src/feature_manager.hpp
    src/startup_manager.hpp
    src/file_watchdog.hpp
    src/package_config.hpp
    src/operation_handler.hpp
    src/init_manager.hpp
    src/rpc_client.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ur-licence-shared
        lcxx_crypto
        lcxx_identifiers
        lcxx_ident_utils
        licensecxx
        lcxx_license
        ${OPENSSL_LIBRARIES}
        ur-rpc-template
        threadmanager
        threadmanager_cpp
        pthread
        stdc++
)

# Additional system libraries for static linking
if(BUILD_STATIC AND UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            dl
            pthread
            rt
    )
endif()

# Compiler features
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Process the cmake subdirectory for header generation
add_subdirectory(cmake)

# Include the build directory for generated headers (device_config.h)
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/cmake)

# Print configuration summary
message(STATUS "========================================")
message(STATUS "ur-licence-mann Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Static linking: ${BUILD_STATIC}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")