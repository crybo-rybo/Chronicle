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
