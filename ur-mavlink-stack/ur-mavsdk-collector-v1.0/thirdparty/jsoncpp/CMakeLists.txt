cmake_minimum_required(VERSION 3.10)
project(jsoncpp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create library target
add_library(jsoncpp_lib STATIC
    src/json_reader.cpp
    src/json_value.cpp
    src/json_writer.cpp
)

# Include directories - point to src directory for <json/*> includes
target_include_directories(jsoncpp_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Public headers (for IDE organization)
target_sources(jsoncpp_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/assertions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/forwards.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/json_features.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/reader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/value.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json/writer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json_tool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json_valueiterator.inl
)

# Installation rules
install(TARGETS jsoncpp_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY src/json/
    DESTINATION include/json
    FILES_MATCHING PATTERN "*.h"
)

install(FILES 
    src/json_tool.h
    src/json_valueiterator.inl
    DESTINATION include/json
)
