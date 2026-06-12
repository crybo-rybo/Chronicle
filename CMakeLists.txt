cmake_minimum_required(VERSION 3.25)
project(chronicle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warning flags for Chronicle-owned targets (not vendored dependencies)
option(CHRONICLE_WERROR "Treat compiler warnings as errors" OFF)
option(CHRONICLE_ENABLE_HARNESS "Build live zoo-keeper-harness LLM integration" ON)
option(CHRONICLE_ENABLE_LOGGING "Enable Chronicle diagnostic logging by default" OFF)
set(CHRONICLE_WARNING_FLAGS -Wall -Wextra -Wpedantic)
if(CHRONICLE_WERROR)
    list(APPEND CHRONICLE_WARNING_FLAGS -Werror)
endif()

# nlohmann/json — fetched before the harness so chronicle_world/scenario can link it
# without pulling the full LLM stack.  The harness dependency reuses this target.
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

if(CHRONICLE_ENABLE_HARNESS)
    FetchContent_Declare(
        zoo-keeper-harness
        GIT_REPOSITORY https://github.com/crybo-rybo/zoo-keeper-harness.git
        GIT_TAG v1.0.0
    )
    FetchContent_MakeAvailable(zoo-keeper-harness)
endif()

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
if(CHRONICLE_BUILD_TOOLS AND CHRONICLE_ENABLE_HARNESS)
    add_subdirectory(tools)
endif()

# TUI renderer (stretch goal, not implemented in Sprint 0)
option(CHRONICLE_TUI "Build with FTXUI TUI renderer" OFF)
