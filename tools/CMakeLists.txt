# Development tools — manual smoke tests and benchmarks, not part of the automated test suite
# Build with: cmake -B build -DCHRONICLE_BUILD_TOOLS=ON

add_executable(harness_smoke_test harness_smoke_test.cpp)
target_link_libraries(harness_smoke_test PRIVATE ZooKeeperHarness::harness)
target_compile_options(harness_smoke_test PRIVATE ${CHRONICLE_WARNING_FLAGS})
