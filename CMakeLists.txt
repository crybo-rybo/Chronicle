cmake_minimum_required(VERSION 3.25)
project(chronicle VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Zoo-Keeper via FetchContent
include(FetchContent)
set(ZOO_FETCH_LLAMA ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    zoo-keeper
    GIT_REPOSITORY https://github.com/crybo-rybo/zoo-keeper.git
    GIT_TAG v1.1.1
    GIT_SHALLOW TRUE
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

# TUI renderer (stretch goal, not implemented in Sprint 0)
option(CHRONICLE_TUI "Build with FTXUI TUI renderer" OFF)
