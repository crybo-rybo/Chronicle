cmake_minimum_required(VERSION 3.28)
project(chronicle VERSION 2.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)

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
option(CHRONICLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CHRONICLE_WARNING_FLAGS -Wall -Wextra -Wpedantic)
if(CHRONICLE_WERROR)
    list(APPEND CHRONICLE_WARNING_FLAGS -Werror)
endif()
set(CHRONICLE_SANITIZER_FLAGS)
if(CHRONICLE_SANITIZERS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "CHRONICLE_SANITIZERS is currently supported on Linux only")
    endif()
    set(CHRONICLE_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

include(FetchContent)

# nlohmann/json — cartridge parsing and save files.
set(JSON_SystemInclude ON CACHE BOOL "" FORCE) # keep its C++26 deprecation noise out of our warnings
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# miniz — .chronicle/.zip cartridge archives for pack/install. The release
# zip is the amalgamated pair (miniz.c/miniz.h) with no CMake project.
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
    URL_HASH SHA256=ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)
add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR})
target_compile_options(miniz PRIVATE ${CHRONICLE_SANITIZER_FLAGS})

# scry — LLM harness; the reflection component generates NPC tool schemas.
set(SCRY_ENABLE_REFLECTION ON CACHE BOOL "" FORCE)
if(CHRONICLE_SANITIZERS)
    set(SCRY_SANITIZER address-undefined CACHE STRING "" FORCE)
else()
    set(SCRY_SANITIZER none CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
    scry
    GIT_REPOSITORY https://github.com/crybo-rybo/scry.git
    # v0.1.0, pinned to an immutable commit for reproducible builds.
    GIT_TAG c499516310a5852c2407e0dd5f3d8a9579fed20c
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(scry)

add_subdirectory(src)

install(TARGETS chronicle RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/chronicle/examples)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY schemas/ DESTINATION ${CMAKE_INSTALL_DATADIR}/chronicle/schemas)
install(FILES README.md CONTRIBUTING.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

if(CHRONICLE_BUILD_TESTS OR CHRONICLE_INTEGRATION_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
