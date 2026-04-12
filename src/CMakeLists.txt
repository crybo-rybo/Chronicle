# Entity structs + serialization — linked by both chronicle and chronicle_tests
# NOTE: Adding or removing .cpp files requires re-running CMake (cmake -B build), not just a rebuild
file(GLOB_RECURSE ENTITY_SOURCES entities/*.cpp)
add_library(chronicle_entities STATIC ${ENTITY_SOURCES})
target_include_directories(chronicle_entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_entities PUBLIC ZooKeeper::zoo)

# Game layers (AI, Engine, Rendering, Persistence)
# NOTE: Adding or removing .cpp files requires re-running CMake (cmake -B build), not just a rebuild
file(GLOB_RECURSE GAME_SOURCES ai/*.cpp engine/*.cpp rendering/*.cpp persistence/*.cpp)
if(GAME_SOURCES)
    add_library(chronicle_game STATIC ${GAME_SOURCES})
    target_include_directories(chronicle_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(chronicle_game PUBLIC chronicle_entities)
endif()

# Main executable
add_executable(chronicle main.cpp)
if(TARGET chronicle_game)
    # chronicle_game links chronicle_entities publicly, so no need to list it again
    target_link_libraries(chronicle PRIVATE chronicle_game)
else()
    target_link_libraries(chronicle PRIVATE chronicle_entities)
endif()
