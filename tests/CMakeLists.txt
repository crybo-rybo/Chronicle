set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    # v1.17.0, pinned to an immutable commit for reproducible builds.
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

if(CHRONICLE_BUILD_TESTS)
    add_executable(chronicle_tests
        test_cartridge.cpp
        test_validator.cpp
        test_game.cpp
        test_npc_tools.cpp
        test_events.cpp
        test_persist.cpp
        test_prompt.cpp
        test_library.cpp
        test_cli.cpp
        test_runtime_stub.cpp
    )
    target_compile_options(chronicle_tests PRIVATE
        ${CHRONICLE_WARNING_FLAGS}
        ${CHRONICLE_SANITIZER_FLAGS}
    )
    target_link_options(chronicle_tests PRIVATE ${CHRONICLE_SANITIZER_FLAGS})
    target_compile_definitions(chronicle_tests PRIVATE
        CHRONICLE_REPO_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(chronicle_tests PRIVATE chronicle_llm GTest::gtest_main)
    gtest_discover_tests(chronicle_tests)
endif()

if(CHRONICLE_INTEGRATION_TESTS)
    add_executable(chronicle_integration_tests
        integration/test_llm_playthrough.cpp
    )
    target_compile_options(chronicle_integration_tests PRIVATE
        ${CHRONICLE_WARNING_FLAGS}
        ${CHRONICLE_SANITIZER_FLAGS}
    )
    target_link_options(chronicle_integration_tests PRIVATE ${CHRONICLE_SANITIZER_FLAGS})
    target_compile_definitions(chronicle_integration_tests PRIVATE
        CHRONICLE_REPO_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(chronicle_integration_tests PRIVATE chronicle_llm GTest::gtest_main)
    gtest_discover_tests(chronicle_integration_tests
        PROPERTIES LABELS integration
        DISCOVERY_TIMEOUT 30
        TEST_PREFIX "integration/")
endif()
