cmake_minimum_required(VERSION 3.16)
project(nsp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict compiler warnings
add_compile_options(-Wall -Wextra -Werror -pedantic)

# Compile-time debug logging option
option(NSP_DEBUG "Enable compile-time debug logging" ON)
if(NSP_DEBUG)
    add_compile_definitions(NSP_COMPILE_DEBUG=1)
else()
    add_compile_definitions(NSP_COMPILE_DEBUG=0)
endif()

# Embedded RPATH configuration: ensures binaries find libnsp.so in current binary directory ($ORIGIN)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH "$ORIGIN")

# Shared Library: libnsp.so
add_library(nsp SHARED
    src/lib/protocol.c
    src/lib/io.c
    src/lib/log.c
)
target_include_directories(nsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Executable: server
add_executable(server
    src/server/main.c
    src/server/server.c
    src/server/handlers.c
)
target_link_libraries(server PRIVATE nsp)

# Executable: client
add_executable(client
    src/client/main.c
    src/client/client.c
)
target_link_libraries(client PRIVATE nsp)

# Executable: evil_client
add_executable(evil_client
    src/evil_client/main.c
)
target_link_libraries(evil_client PRIVATE nsp)

# Testing
enable_testing()

add_executable(test_framing tests/test_framing.c)
target_link_libraries(test_framing PRIVATE nsp)
add_test(NAME test_framing COMMAND test_framing)

add_executable(test_log tests/test_log.c)
target_link_libraries(test_log PRIVATE nsp)
add_test(NAME test_log COMMAND test_log)

add_executable(test_e2e_echo_quit tests/test_e2e_echo_quit.c)
target_link_libraries(test_e2e_echo_quit PRIVATE nsp)
add_test(NAME test_e2e_echo_quit COMMAND test_e2e_echo_quit)

add_executable(test_sysinfo_concurrency tests/test_sysinfo_concurrency.c)
target_link_libraries(test_sysinfo_concurrency PRIVATE nsp)
add_test(NAME test_sysinfo_concurrency COMMAND test_sysinfo_concurrency)

add_executable(test_robustness tests/test_robustness.c)
target_link_libraries(test_robustness PRIVATE nsp)
add_test(NAME test_robustness COMMAND test_robustness)
