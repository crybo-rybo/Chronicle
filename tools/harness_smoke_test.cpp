/// @file harness_smoke_test.cpp
/// @brief Manual smoke test for zoo-keeper-harness endpoint chat.
/// Build with: cmake -B build -DCHRONICLE_BUILD_TOOLS=ON && cmake --build build
/// Run with:   ZOO_BASE_URL=http://localhost:11434/v1 ZOO_MODEL=model
/// ./build/tools/harness_smoke_test

#include <zoo/Agent.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::string env_or(const char *name, const char *fallback) {
    if (const char *value = std::getenv(name)) {
        return value;
    }
    return fallback;
}

} // namespace

int main() {
    zoo::EndpointConfig endpoint;
    endpoint.base_url = env_or("ZOO_BASE_URL", "http://localhost:11434/v1");
    endpoint.model = env_or("ZOO_MODEL", "");
    endpoint.api_key = env_or("ZOO_API_KEY", "");
    if (endpoint.api_key.empty()) {
        endpoint.api_key = env_or("OPENAI_API_KEY", "");
    }

    if (endpoint.model.empty()) {
        std::cerr << "Error: ZOO_MODEL is not set.\n"
                  << "Usage: ZOO_BASE_URL=http://localhost:11434/v1 "
                     "ZOO_MODEL=<model> [ZOO_API_KEY=<key>] ./harness_smoke_test\n";
        return 1;
    }

    zoo::GenerationOptions options;
    options.max_tokens = 128;

    auto agent = zoo::Agent::Builder{}
                     .endpoint(std::move(endpoint))
                     .system_prompt("Answer briefly.")
                     .default_options(options)
                     .build();
    if (!agent) {
        std::cerr << "Error: Failed to create harness agent: " << agent.error().to_string() << "\n";
        return 1;
    }

    std::cout << "Agent created successfully. Sending chat message...\n\n";

    auto on_token = [](std::string_view token) -> zoo::TokenAction {
        std::cout << token << std::flush;
        return zoo::TokenAction::Continue;
    };

    auto result = agent->generate("Hello, who are you?", {}, on_token);
    if (!result) {
        std::cerr << "\nError: Chat request failed: " << result.error().to_string() << "\n";
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
