cmake_minimum_required(VERSION 3.10)
project(ur-wg-provider C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wold-style-definition -Wstrict-prototypes -Wno-stringop-truncation")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-g -O2")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LIBRARY_SOURCES
    src/config.c
    src/encoding.c
    src/curve25519.c
    src/ipc.c
    src/terminal.c
    src/routing.c
)

add_library(wg-provider STATIC ${LIBRARY_SOURCES})

target_compile_definitions(wg-provider PRIVATE
    _GNU_SOURCE
    RUNSTATEDIR="/var/run"
)

add_executable(wg-tunnel main.c)

target_link_libraries(wg-tunnel wg-provider)

set_target_properties(wg-provider PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set_target_properties(wg-tunnel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS wg-provider
    ARCHIVE DESTINATION lib
)

install(TARGETS wg-tunnel
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/ur-wg-provider
    FILES_MATCHING PATTERN "*.h"
)