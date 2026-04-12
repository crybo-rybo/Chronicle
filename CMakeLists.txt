cmake_minimum_required(VERSION 3.25)
project(chronicle VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Zoo-Keeper via FetchContent
include(FetchContent)
FetchContent_Declare(
    zoo-keeper
    GIT_REPOSITORY https://github.com/crybo-rybo/zoo-keeper.git
    GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(zoo-keeper)

# Main executable
add_subdirectory(src)

# Tests
option(CHRONICLE_BUILD_TESTS "Build tests" ON)
if(CHRONICLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Development tools (smoke tests, benchmarks)
option(CHRONICLE_BUILD_TOOLS "Build development tools" OFF)
if(CHRONICLE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# TUI renderer (stretch goal, not implemented in Sprint 0)
option(CHRONICLE_TUI "Build with FTXUI TUI renderer" OFF)
