cmake_minimum_required(VERSION 3.20)

project(ShotCallerWow)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SHOTCALLER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SHOTCALLER_ENABLE_SANITIZERS
       "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Diagnostics shared by every compiled target in the project.
add_library(shotcaller_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(shotcaller_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )
    if(SHOTCALLER_WARNINGS_AS_ERRORS)
        target_compile_options(shotcaller_warnings INTERFACE -Werror)
    endif()
    if(SHOTCALLER_ENABLE_SANITIZERS)
        target_compile_options(shotcaller_warnings INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(shotcaller_warnings INTERFACE -fsanitize=address,undefined)
    endif()
endif()

add_executable(ShotCallerWow src/main.cpp src/parser.cpp src/line_reader.cpp src/engine.cpp
                             src/line_writer.cpp src/socket_sender.cpp)
target_include_directories(ShotCallerWow PUBLIC include)
target_link_libraries(ShotCallerWow PRIVATE shotcaller_warnings)

# Static library for testable modules (no I/O)
add_library(ShotCallerLib STATIC src/parser.cpp src/engine.cpp)
target_include_directories(ShotCallerLib PUBLIC include)
target_link_libraries(ShotCallerLib PRIVATE shotcaller_warnings)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(ShotCallerTests
    tests/test_constants.cpp
    tests/test_parser.cpp
    tests/test_engine.cpp
)
target_link_libraries(ShotCallerTests PRIVATE ShotCallerLib GTest::gtest_main
                                             shotcaller_warnings)

include(GoogleTest)
gtest_discover_tests(ShotCallerTests)
