# chronicle_core: deterministic game — cartridges, mechanics, persistence. No LLM.
add_library(chronicle_core STATIC
    chronicle/cartridge/models.cpp
    chronicle/cartridge/loader.cpp
    chronicle/cartridge/validator.cpp
    chronicle/game/cartridge_game.cpp
    chronicle/game/world_actions.cpp
    chronicle/library.cpp
    chronicle/persist.cpp
    chronicle/prompt.cpp
    chronicle/render.cpp
)
target_include_directories(chronicle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(chronicle_core PRIVATE ${CHRONICLE_WARNING_FLAGS})
target_link_libraries(chronicle_core PUBLIC nlohmann_json::nlohmann_json miniz)

# chronicle_llm: scry harness integration + console runtime + CLI.
add_library(chronicle_llm STATIC
    chronicle/llm/npc_sessions.cpp
    chronicle/runtime.cpp
    chronicle/cli.cpp
)
target_compile_options(chronicle_llm PRIVATE ${CHRONICLE_WARNING_FLAGS} -freflection)
target_link_libraries(chronicle_llm PUBLIC chronicle_core scry::scry scry::reflection)

add_executable(chronicle main.cpp)
target_compile_options(chronicle PRIVATE ${CHRONICLE_WARNING_FLAGS})
target_link_libraries(chronicle PRIVATE chronicle_llm)
