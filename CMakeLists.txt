cmake_minimum_required(VERSION 3.20)
project(tokoro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compile_commands.json for clang-tidy/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warnings
set(WARNING_FLAGS 
    -Wall 
    -Wextra 
    -Wpedantic 
    -Wshadow 
    -Wnon-virtual-dtor 
    -Wold-style-cast 
    -Wcast-align 
    -Wunused 
    -Woverloaded-virtual 
    -Wpedantic 
    -Wconversion 
    -Wsign-conversion
)

# Debug build flags with sanitizers
set(DEBUG_FLAGS
    -g
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
)

add_compile_options("$<$<CONFIG:Debug>:${DEBUG_FLAGS}>")
add_link_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

add_compile_options(${WARNING_FLAGS})

# Add source files
set(SOURCES
    src/main.cpp
    src/socket.cpp
    src/server.cpp
    src/thread_pool.cpp
    src/http_parser.cpp
    src/file_handler.cpp
    src/file_handler.cpp
)

add_executable(tokoro ${SOURCES})
target_include_directories(tokoro PRIVATE src)

# Add testing support via Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

set(TEST_SOURCES
    tests/test_http_parser.cpp
    tests/test_file_handler.cpp
    tests/test_thread_pool.cpp
    tests/test_socket.cpp
    src/http_parser.cpp
    src/file_handler.cpp
    src/thread_pool.cpp
    src/socket.cpp
)

add_executable(tokoro_tests ${TEST_SOURCES})
target_include_directories(tokoro_tests PRIVATE src)
target_link_libraries(tokoro_tests PRIVATE Catch2::Catch2WithMain)

# Add tests
include(Catch)
catch_discover_tests(tokoro_tests)
