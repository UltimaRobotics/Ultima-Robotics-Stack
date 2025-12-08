cmake_minimum_required(VERSION 3.10)
project(iperf3 VERSION 3.19.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

check_include_file(poll.h HAVE_POLL_H)
check_include_file(linux/tcp.h HAVE_LINUX_TCP_H)
check_include_file(endian.h HAVE_ENDIAN_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
check_include_file(stdatomic.h HAVE_STDATOMIC_H)
check_include_file(netinet/sctp.h HAVE_NETINET_SCTP_H)

check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(clock_nanosleep HAVE_CLOCK_NANOSLEEP)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(daemon HAVE_DAEMON)
check_function_exists(sendfile HAVE_SENDFILE)
check_function_exists(getline HAVE_GETLINE)
check_function_exists(sched_setaffinity HAVE_SCHED_SETAFFINITY)
check_function_exists(cpuset_setaffinity HAVE_CPUSET_SETAFFINITY)

if(HAVE_SCHED_SETAFFINITY OR HAVE_CPUSET_SETAFFINITY)
    set(HAVE_CPU_AFFINITY 1)
endif()

check_c_source_compiles("
    #include <netinet/tcp.h>
    int main() { int foo = TCP_CONGESTION; return 0; }
" HAVE_TCP_CONGESTION)

check_c_source_compiles("
    #include <netinet/tcp.h>
    int main() { int foo = TCP_USER_TIMEOUT; return 0; }
" HAVE_TCP_USER_TIMEOUT)

check_c_source_compiles("
    #include <netinet/tcp.h>
    int main() { int foo = TCP_KEEPIDLE; return 0; }
" HAVE_TCP_KEEPALIVE)

check_c_source_compiles("
    #include <sys/types.h>
    #include <linux/in6.h>
    int main() { int foo = IPV6_FLOWLABEL_MGR; return 0; }
" HAVE_FLOWLABEL)

check_c_source_compiles("
    #include <sys/socket.h>
    int main() { int foo = SO_MAX_PACING_RATE; return 0; }
" HAVE_SO_MAX_PACING_RATE)

check_c_source_compiles("
    #include <sys/socket.h>
    int main() { int foo = SO_BINDTODEVICE; return 0; }
" HAVE_SO_BINDTODEVICE)

check_c_source_compiles("
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    int main() { int foo = IP_MTU_DISCOVER; return 0; }
" HAVE_IP_MTU_DISCOVER)

check_c_source_compiles("
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    int main() { int foo = IP_DONTFRAG; return 0; }
" HAVE_IP_DONTFRAG)

check_c_source_compiles("
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    int main() { int foo = IP_DONTFRAGMENT; return 0; }
" HAVE_IP_DONTFRAGMENT)

if(HAVE_IP_MTU_DISCOVER OR HAVE_IP_DONTFRAG OR HAVE_IP_DONTFRAGMENT)
    set(HAVE_DONT_FRAGMENT 1)
endif()

check_c_source_compiles("
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    int main() { int foo = MSG_TRUNC; return 0; }
" HAVE_MSG_TRUNC)

check_c_source_compiles("
    #include <netinet/in.h>
    int main() { int foo = IPPROTO_MPTCP; return 0; }
" HAVE_IPPROTO_MPTCP)

check_c_source_compiles("
    #ifdef HAVE_LINUX_TCP_H
    #include <linux/tcp.h>
    #else
    #include <sys/types.h>
    #include <netinet/tcp.h>
    #endif
    int main() { 
        struct tcp_info ti;
        ti.tcpi_snd_wnd = 0;
        return 0; 
    }
" HAVE_TCP_INFO_SND_WND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/iperf_config.h.in 
               ${CMAKE_CURRENT_BINARY_DIR}/iperf_config.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.h)

set(LIBIPERF_SOURCES
    src/cjson.c
    src/dscp.c
    src/iperf_api.c
    src/iperf_auth.c
    src/iperf_client_api.c
    src/iperf_error.c
    src/iperf_locale.c
    src/iperf_pthread.c
    src/iperf_sctp.c
    src/iperf_server_api.c
    src/iperf_tcp.c
    src/iperf_time.c
    src/iperf_udp.c
    src/iperf_util.c
    src/net.c
    src/tcp_info.c
    src/timer.c
    src/units.c
)

add_library(iperf SHARED ${LIBIPERF_SOURCES})
add_library(iperf_static STATIC ${LIBIPERF_SOURCES})

set_target_properties(iperf_static PROPERTIES OUTPUT_NAME iperf)

target_include_directories(iperf PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(iperf_static PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(iperf PRIVATE 
    HAVE_PTHREAD=1
    HAVE_SSL=1
)

target_compile_definitions(iperf_static PRIVATE 
    HAVE_PTHREAD=1
    HAVE_SSL=1
)

target_link_libraries(iperf 
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    m
)

target_link_libraries(iperf_static 
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    m
)

add_executable(iperf3 src/main.c)
target_link_libraries(iperf3 iperf)
target_include_directories(iperf3 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    add_executable(t_timer src/t_timer.c)
    target_link_libraries(t_timer iperf)
    target_include_directories(t_timer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(t_units src/t_units.c)
    target_link_libraries(t_units iperf)
    target_include_directories(t_units PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(t_uuid src/t_uuid.c)
    target_link_libraries(t_uuid iperf)
    target_include_directories(t_uuid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(t_api src/t_api.c)
    target_link_libraries(t_api iperf)
    target_include_directories(t_api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(t_auth src/t_auth.c)
    target_link_libraries(t_auth iperf)
    target_include_directories(t_auth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
endif()

option(BUILD_EXAMPLES "Build example programs" OFF)
if(BUILD_EXAMPLES)
    add_executable(mic examples/mic.c)
    target_link_libraries(mic iperf)
    target_include_directories(mic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(mis examples/mis.c)
    target_link_libraries(mis iperf)
    target_include_directories(mis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
endif()

install(TARGETS iperf iperf3
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES src/iperf_api.h DESTINATION include)

# Always build the C++ wrapper for ur-netbench-mann integration
add_subdirectory(cpp-wrapper)
