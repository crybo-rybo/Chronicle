include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# Gather all test_*.cpp files automatically
file(GLOB TEST_SOURCES test_*.cpp)
add_executable(chronicle_tests ${TEST_SOURCES})
target_link_libraries(chronicle_tests PRIVATE GTest::gtest_main chronicle_entities)

# Copy fixture files to build directory so tests can find them
# NOTE: Copies at configure time only — re-run CMake (cmake -B build) when adding new fixture files
file(COPY fixtures/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

# Define FIXTURES_DIR macro so tests can reference fixtures at a known path
target_compile_definitions(chronicle_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
)

include(GoogleTest)
gtest_discover_tests(chronicle_tests)
