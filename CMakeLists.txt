cmake_minimum_required(VERSION 3.20)

project(sys-toolkit-c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic -fstack-protector-all")

# Custom sanitizer build types
set(CMAKE_C_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address")
set(CMAKE_C_FLAGS_UBSAN "-O1 -g -fsanitize=undefined -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_UBSAN "-fsanitize=undefined")

set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug)

include(FetchContent)

option(SYS_TOOLKIT_ENABLE_TESTS "Build tests" ON)
option(SYS_TOOLKIT_ENABLE_BENCHMARKS "Build benchmarks" ON)

add_library(sys_toolkit
    src/sys_arena.c
    src/sys_threadpool.c
    src/sys_log.c
)

target_include_directories(sys_toolkit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(sys_toolkit PUBLIC Threads::Threads)

enable_testing()

if(SYS_TOOLKIT_ENABLE_TESTS)
    add_executable(sys_toolkit_tests
        tests/test_main.c
        tests/test_sys_arena.c
        tests/test_sys_threadpool.c
        tests/test_sys_log.c
    )
    target_link_libraries(sys_toolkit_tests PRIVATE sys_toolkit)
    add_test(NAME sys_toolkit_tests COMMAND sys_toolkit_tests)
endif()

if(SYS_TOOLKIT_ENABLE_BENCHMARKS)
    add_executable(sys_toolkit_benchmarks
        benchmarks/arena_bench.c
    )
    target_link_libraries(sys_toolkit_benchmarks PRIVATE sys_toolkit)
endif()

# clang-format integration (best-effort)
find_program(CLANG_FORMAT_BIN clang-format)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE SYS_TOOLKIT_FORMAT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.c"
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i ${SYS_TOOLKIT_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on C sources"
        VERBATIM
    )

    if(TARGET sys_toolkit_tests)
        add_dependencies(sys_toolkit_tests format)
    endif()
    if(TARGET sys_toolkit_benchmarks)
        add_dependencies(sys_toolkit_benchmarks format)
    endif()
    add_dependencies(sys_toolkit format)
endif()

