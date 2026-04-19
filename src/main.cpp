/**
 * @file main.cpp
 * @brief Entry point for the Chronicle text adventure game.
 *
 * @details Bootstraps the runtime and launches the @ref GameEngine.
 * Configuration is loaded from @c config/default.json and world data
 * from the @c data/ directory, both resolved relative to the working
 * directory.  If either path is missing or malformed, the error is
 * reported and the process exits with a non-zero status.
 */

#include "diagnostics/logger.hpp"
#include "engine/game_engine.hpp"
#include <exception>
#include <iostream>

int main() {
    chronicle::logging::configure_from_environment();
    try {
        chronicle::GameEngine engine("config/default.json", "data", nullptr);
        engine.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
