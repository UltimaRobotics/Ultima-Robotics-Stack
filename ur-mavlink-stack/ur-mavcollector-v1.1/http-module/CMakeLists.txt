
cmake_minimum_required(VERSION 3.10)

# Find libcurl
find_package(CURL REQUIRED)

# HTTP module sources
set(HTTP_CLIENT_SOURCES
    src/HttpClient.cpp
)

# HTTP module headers
set(HTTP_CLIENT_HEADERS
    include/HttpClient.hpp
)

# Create library
add_library(http_client STATIC ${HTTP_CLIENT_SOURCES})

target_include_directories(http_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../nholmann
)

target_link_libraries(http_client
    PUBLIC
        ${CURL_LIBRARIES}
)

# Export variables for parent project
set(HTTP_CLIENT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(HTTP_CLIENT_LIBS http_client ${CURL_LIBRARIES} PARENT_SCOPE)
