# Entity structs + serialization — linked by both chronicle and chronicle_tests
file(GLOB_RECURSE ENTITY_SOURCES entities/*.cpp)
add_library(chronicle_entities STATIC ${ENTITY_SOURCES})
target_include_directories(chronicle_entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_entities PUBLIC ZooKeeper::zoo)

# Main executable
add_executable(chronicle main.cpp)
target_link_libraries(chronicle PRIVATE chronicle_entities)
