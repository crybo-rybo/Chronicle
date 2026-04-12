# Development tools — manual smoke tests and benchmarks, not part of the automated test suite
# Build with: cmake -B build -DCHRONICLE_BUILD_TOOLS=ON

add_executable(zk_smoke_test zk_smoke_test.cpp)
target_link_libraries(zk_smoke_test PRIVATE ZooKeeper::zoo)
