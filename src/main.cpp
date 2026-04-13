/**
 * @file main.cpp
 * @brief Entry point for the Chronicle text adventure game.
 *
 * @details Bootstraps the runtime, verifies the Zoo-Keeper library is available,
 * and will eventually initialize and run the GameEngine once the full startup
 * pipeline is wired together.  For now it serves as a build-smoke-test and
 * version banner.
 */

#include <iostream>

#include <zoo/zoo.hpp>

int main() {
    std::cout << "Chronicle v0.1.0" << std::endl;
    std::cout << "An LLM-driven text adventure" << std::endl;
    std::cout << "Powered by Zoo-Keeper v" << zoo::VERSION_STRING << std::endl;
    std::cout << "Roles: " << zoo::role_to_string(zoo::Role::User) << ", "
              << zoo::role_to_string(zoo::Role::Assistant) << std::endl;
    return 0;
}
