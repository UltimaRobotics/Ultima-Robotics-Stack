cmake_minimum_required(VERSION 3.10)
project(ur-mavrouter-shared)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/nholmann
)

# Source files
set(SHARED_SOURCES
    ThreadRpcTypes.cpp
    ExtensionRpcTypes.cpp
    RpcMessageTypes.cpp
)

# Header files
set(SHARED_HEADERS
    ThreadRpcTypes.hpp
    ExtensionRpcTypes.hpp
    RpcMessageTypes.hpp
)

# Create shared library
add_library(ur-mavrouter-shared SHARED ${SHARED_SOURCES} ${SHARED_HEADERS})

# Create static library
add_library(ur-mavrouter-shared-static STATIC ${SHARED_SOURCES} ${SHARED_HEADERS})

# Set library properties
set_target_properties(ur-mavrouter-shared PROPERTIES
    VERSION 1.0
    SOVERSION 1
    PUBLIC_HEADER "${SHARED_HEADERS}"
)

set_target_properties(ur-mavrouter-shared-static PROPERTIES
    PUBLIC_HEADER "${SHARED_HEADERS}"
)

# Link libraries (if needed)
target_link_libraries(ur-mavrouter-shared pthread)
target_link_libraries(ur-mavrouter-shared-static pthread)

# Create export target
install(TARGETS ur-mavrouter-shared ur-mavrouter-shared-static
    EXPORT ur-mavrouter-shared-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/ur-mavrouter-shared
)

# Export targets
install(EXPORT ur-mavrouter-shared-targets
    FILE ur-mavrouter-shared-targets.cmake
    NAMESPACE ur-mavrouter::
    DESTINATION lib/cmake/ur-mavrouter-shared
)
