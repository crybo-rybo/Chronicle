cmake_minimum_required(VERSION 3.25)
project(chronicle VERSION 2.0.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR "Chronicle requires GCC 16+ (C++26 reflection via -freflection). "
                        "Found: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CHRONICLE_WERROR "Treat compiler warnings as errors" OFF)
option(CHRONICLE_BUILD_TESTS "Build unit tests" ON)
option(CHRONICLE_INTEGRATION_TESTS "Build integration tests that require a local LLM model" OFF)

set(CHRONICLE_WARNING_FLAGS -Wall -Wextra -Wpedantic)
if(CHRONICLE_WERROR)
    list(APPEND CHRONICLE_WARNING_FLAGS -Werror)
endif()

include(FetchContent)

# nlohmann/json — cartridge parsing and save files.
set(JSON_SystemInclude ON CACHE BOOL "" FORCE) # keep its C++26 deprecation noise out of our warnings
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

# miniz — .chronicle/.zip cartridge archives for pack/install. The release
# zip is the amalgamated pair (miniz.c/miniz.h) with no CMake project.
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
)
FetchContent_MakeAvailable(miniz)
add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR})

# scry — LLM harness; the reflection component generates NPC tool schemas.
set(SCRY_ENABLE_REFLECTION ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    scry
    GIT_REPOSITORY https://github.com/crybo-rybo/scry.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(scry)

add_subdirectory(src)

if(CHRONICLE_BUILD_TESTS OR CHRONICLE_INTEGRATION_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
