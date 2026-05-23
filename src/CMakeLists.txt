# chronicle_common — header-only shared utilities (parse helpers, etc.)
add_library(chronicle_common INTERFACE)
target_include_directories(chronicle_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# chronicle_diagnostics — opt-in logging used by Chronicle runtime layers
add_library(chronicle_diagnostics STATIC diagnostics/logger.cpp)
target_include_directories(chronicle_diagnostics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(chronicle_diagnostics PRIVATE ${CHRONICLE_WARNING_FLAGS})
if(CHRONICLE_ENABLE_LOGGING)
    target_compile_definitions(chronicle_diagnostics PUBLIC CHRONICLE_ENABLE_LOGGING=1)
else()
    target_compile_definitions(chronicle_diagnostics PUBLIC CHRONICLE_ENABLE_LOGGING=0)
endif()

# chronicle_world — runtime world state + entity serialization
set(WORLD_SOURCES
    entities/clock.cpp
    entities/event.cpp
    entities/fact.cpp
    entities/flag.cpp
    entities/item.cpp
    entities/location.cpp
    entities/npc.cpp
    entities/player.cpp
    entities/world.cpp
)
add_library(chronicle_world STATIC ${WORLD_SOURCES})
target_include_directories(chronicle_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_world PUBLIC chronicle_common nlohmann_json::nlohmann_json)
target_compile_options(chronicle_world PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_scenario — cartridge I/O, config, loading, and validation
set(SCENARIO_SOURCES
    entities/config.cpp
    entities/scenario.cpp
    entities/world_loader.cpp
    entities/world_validator.cpp
)
add_library(chronicle_scenario STATIC ${SCENARIO_SOURCES})
target_include_directories(chronicle_scenario PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_scenario PUBLIC chronicle_world chronicle_common nlohmann_json::nlohmann_json)
target_compile_options(chronicle_scenario PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_engine — engine layer (mutations, command parser, token queue, mutation gate)
# Excludes game_engine.{hpp,cpp} which lives in chronicle_runtime.
file(GLOB ENGINE_SOURCES engine/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "game_engine\\.cpp$")
add_library(chronicle_engine STATIC ${ENGINE_SOURCES})
target_include_directories(chronicle_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_engine PUBLIC chronicle_scenario chronicle_common chronicle_diagnostics)
target_compile_options(chronicle_engine PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_ai — AI layer (Zoo-Keeper adapter, prompt builder, tool registry, response handler)
file(GLOB AI_SOURCES ai/*.cpp)
if(NOT CHRONICLE_ENABLE_ZOO)
    list(FILTER AI_SOURCES EXCLUDE REGEX "zoo_agent_adapter\\.cpp$")
endif()
add_library(chronicle_ai STATIC ${AI_SOURCES})
target_include_directories(chronicle_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_ai PUBLIC chronicle_engine chronicle_diagnostics)
if(CHRONICLE_ENABLE_ZOO)
    target_link_libraries(chronicle_ai PUBLIC ZooKeeper::zoo)
    target_compile_definitions(chronicle_ai PUBLIC CHRONICLE_ENABLE_ZOO=1)
else()
    target_compile_definitions(chronicle_ai PUBLIC CHRONICLE_ENABLE_ZOO=0)
endif()
target_compile_options(chronicle_ai PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_rendering — renderer interfaces and terminal renderer
file(GLOB RENDERING_SOURCES rendering/*.cpp)
add_library(chronicle_rendering STATIC ${RENDERING_SOURCES})
target_include_directories(chronicle_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_rendering PUBLIC chronicle_world)
target_compile_options(chronicle_rendering PRIVATE ${CHRONICLE_WARNING_FLAGS})

# chronicle_persistence — save/load system
file(GLOB PERSISTENCE_SOURCES persistence/*.cpp)
add_library(chronicle_persistence STATIC ${PERSISTENCE_SOURCES})
target_include_directories(chronicle_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronicle_persistence PUBLIC chronicle_world nlohmann_json::nlohmann_json)
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
