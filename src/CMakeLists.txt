# chronicle_domain — entity structs + JSON serialization (no AI, no Zoo-Keeper)
# NOTE: Adding or removing .cpp files requires re-running CMake (cmake -B build)
file(GLOB DOMAIN_SOURCES entities/*.cpp)
add_library(chronicle_domain STATIC ${DOMAIN_SOURCES})
target_include_directories(chronicle_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_domain PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(chronicle_domain PRIVATE ${CHRONICLE_WARNING_FLAGS})

# Backward-compat alias so downstream targets can still reference chronicle_entities
add_library(chronicle_entities ALIAS chronicle_domain)

# chronicle_engine — engine layer (mutations, command parser, token queue, mutation gate)
# Excludes game_engine.{hpp,cpp} which lives in chronicle_runtime.
file(GLOB ENGINE_SOURCES engine/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "game_engine\\.cpp$")
add_library(chronicle_engine STATIC ${ENGINE_SOURCES})
target_include_directories(chronicle_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_engine PUBLIC chronicle_domain)
target_compile_options(chronicle_engine PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_ai — AI layer (Zoo-Keeper adapter, prompt builder, tool registry, response handler)
file(GLOB AI_SOURCES ai/*.cpp)
add_library(chronicle_ai STATIC ${AI_SOURCES})
target_include_directories(chronicle_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_ai PUBLIC chronicle_engine ZooKeeper::zoo)
target_compile_options(chronicle_ai PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_rendering — renderer interfaces and terminal renderer
file(GLOB RENDERING_SOURCES rendering/*.cpp)
add_library(chronicle_rendering STATIC ${RENDERING_SOURCES})
target_include_directories(chronicle_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_rendering PUBLIC chronicle_domain)
target_compile_options(chronicle_rendering PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_persistence — save/load system
file(GLOB PERSISTENCE_SOURCES persistence/*.cpp)
add_library(chronicle_persistence STATIC ${PERSISTENCE_SOURCES})
target_include_directories(chronicle_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_persistence PUBLIC chronicle_domain nlohmann_json::nlohmann_json)
target_compile_options(chronicle_persistence PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_runtime — GameEngine orchestrator (composition root library)
add_library(chronicle_runtime STATIC engine/game_engine.cpp)
target_include_directories(chronicle_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_runtime PUBLIC
    chronicle_ai
    chronicle_engine
    chronicle_rendering
    chronicle_persistence
)
target_compile_options(chronicle_runtime PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle executable — thin entry point
add_executable(chronicle main.cpp)
target_compile_options(chronicle PRIVATE ${CHRONICLE_WARNING_FLAGS})
target_link_libraries(chronicle PRIVATE chronicle_runtime)
