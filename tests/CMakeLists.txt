include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# Unit tests (always compiled)
file(GLOB TEST_SOURCES test_*.cpp)

# Integration tests (model-backed, opt-in)
option(CHRONICLE_INTEGRATION_TESTS "Build integration tests that require a local LLM model" OFF)
if(CHRONICLE_INTEGRATION_TESTS)
    file(GLOB INTEGRATION_SOURCES integration/test_*.cpp)
    list(APPEND TEST_SOURCES ${INTEGRATION_SOURCES})
endif()

add_executable(chronicle_tests ${TEST_SOURCES})
target_compile_options(chronicle_tests PRIVATE ${CHRONICLE_WARNING_FLAGS})
target_link_libraries(chronicle_tests PRIVATE GTest::gtest_main chronicle_runtime)

# Copy fixture files to build directory so tests can find them
# NOTE: Copies at configure time only — re-run CMake (cmake -B build) when adding new fixture files
file(COPY fixtures/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

# Define FIXTURES_DIR macro so tests can reference fixtures at a known path
target_compile_definitions(chronicle_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
    CHRONICLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(chronicle_tests)
