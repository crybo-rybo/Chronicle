/// @file zk_smoke_test.cpp
/// @brief Manual smoke test for Zoo-Keeper agent creation and streaming chat.
/// Build with: cmake -B build -DCHRONICLE_BUILD_TOOLS=ON && cmake --build build
/// Run with:   ZOO_MODEL_PATH=/path/to/model.gguf ./build/zk_smoke_test

#include <zoo/agent.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    const char *model_path = std::getenv("ZOO_MODEL_PATH");
    if (!model_path) {
        std::cerr << "Error: ZOO_MODEL_PATH environment variable is not set.\n"
                  << "Usage: ZOO_MODEL_PATH=/path/to/model.gguf ./zk_smoke_test\n";
        return 1;
    }

    std::cout << "Loading model: " << model_path << "\n";

    zoo::ModelConfig model_config{.model_path = model_path, .context_size = 2048};

    auto agent_result = zoo::Agent::create(model_config);
    if (!agent_result) {
        std::cerr << "Error: Failed to create agent: " << agent_result.error().message << "\n";
        return 1;
    }

    auto &agent = *agent_result;
    std::cout << "Agent created successfully. Sending chat message...\n\n";

    auto handle = agent->chat("Hello, who are you?", {},
                              [](std::string_view token) { std::cout << token << std::flush; });

    auto result = handle.await_result();
    if (!result) {
        std::cerr << "\nError: Chat request failed: " << result.error().message << "\n";
        return 1;
    }

    std::cout << "\n\n--- Usage Stats ---\n"
              << "Prompt tokens:     " << result->usage.prompt_tokens << "\n"
              << "Completion tokens: " << result->usage.completion_tokens << "\n"
              << "Tokens/sec:        " << result->metrics.tokens_per_second << "\n"
              << "Time to first:     " << result->metrics.time_to_first_token_ms.count() << " ms\n";

    std::cout << "\nSmoke test passed.\n";
    return 0;
}
