include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(chronicle_tests placeholder_test.cpp)
target_link_libraries(chronicle_tests PRIVATE GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(chronicle_tests)
