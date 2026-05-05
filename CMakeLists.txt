cmake_minimum_required(VERSION 3.25)
project(chronicle VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warning flags for Chronicle-owned targets (not vendored dependencies)
option(CHRONICLE_WERROR "Treat compiler warnings as errors" OFF)
option(CHRONICLE_ENABLE_LOGGING "Enable Chronicle diagnostic logging by default" OFF)
option(CHRONICLE_ENABLE_ZOO_LOGGING "Enable Zoo-Keeper logging when Chronicle logging is enabled" ON)
set(CHRONICLE_WARNING_FLAGS -Wall -Wextra -Wpedantic)
if(CHRONICLE_WERROR)
    list(APPEND CHRONICLE_WARNING_FLAGS -Werror)
endif()

if(CHRONICLE_ENABLE_LOGGING AND CHRONICLE_ENABLE_ZOO_LOGGING)
    set(ZOO_ENABLE_LOGGING ON CACHE BOOL "Enable Zoo-Keeper debug logging to stderr" FORCE)
endif()

# nlohmann/json — fetched before Zoo-Keeper so chronicle_domain can link it
# without pulling the full LLM stack.  Zoo-Keeper's FetchDependencies.cmake
# uses the same FetchContent name ("json"), so this satisfies both.
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Zoo-Keeper via FetchContent
FetchContent_Declare(
    zoo-keeper
    GIT_REPOSITORY https://github.com/crybo-rybo/zoo-keeper.git
    GIT_TAG v1.1.4
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
