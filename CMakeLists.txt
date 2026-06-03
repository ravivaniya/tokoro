cmake_minimum_required(VERSION 3.20)
project(tokoro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compile_commands.json for clang-tidy/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TOKORO_WERROR "Treat warnings as errors" OFF)

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
if(TOKORO_WERROR)
    list(APPEND WARNING_FLAGS -Werror)
endif()

# Debug build flags with sanitizers
set(DEBUG_FLAGS
    -g
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
)

# Core library source files
set(CORE_SOURCES
    src/socket.cpp
    src/server.cpp
    src/thread_pool.cpp
    src/http_parser.cpp
    src/file_handler.cpp
    src/config.cpp
    src/logger.cpp
    src/metrics.cpp
)

# Git version generation
set(VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp")
add_custom_target(
    generate_version ALL
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DDEFAULT_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_version.cmake
    BYPRODUCTS ${VERSION_FILE}
)

add_library(tokoro_core STATIC ${CORE_SOURCES})
target_include_directories(tokoro_core PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_dependencies(tokoro_core generate_version)

target_compile_options(tokoro_core PRIVATE ${WARNING_FLAGS} "$<$<CONFIG:Debug>:${DEBUG_FLAGS}>")
target_link_options(tokoro_core PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

# Main executable
add_executable(tokoro src/main.cpp)
target_link_libraries(tokoro PRIVATE tokoro_core)
target_compile_options(tokoro PRIVATE ${WARNING_FLAGS} "$<$<CONFIG:Debug>:${DEBUG_FLAGS}>")
target_link_options(tokoro PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

# Add testing support via Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

set(TEST_SOURCES
    tests/test_http_parser.cpp
    tests/test_file_handler.cpp
    tests/test_thread_pool.cpp
    tests/test_socket.cpp
)

add_executable(tokoro_tests ${TEST_SOURCES})
target_link_libraries(tokoro_tests PRIVATE tokoro_core Catch2::Catch2WithMain)
target_compile_options(tokoro_tests PRIVATE ${WARNING_FLAGS} "$<$<CONFIG:Debug>:${DEBUG_FLAGS}>")
target_link_options(tokoro_tests PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

# Add tests
include(Catch)
catch_discover_tests(tokoro_tests)
