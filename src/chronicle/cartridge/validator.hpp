// Structural validation for scenario packages.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "chronicle/cartridge/models.hpp"

namespace chronicle {

enum class IssueLevel { error, warning };

struct ValidationIssue {
    std::string message;
    IssueLevel level = IssueLevel::error;

    [[nodiscard]] std::string to_string() const {
        return (level == IssueLevel::error ? "[error] " : "[warning] ") + message;
    }
};

// Cartridge IDs become directory names in the library and save tree. Keep the
// grammar deliberately narrower than a generic filename.
[[nodiscard]] bool is_safe_cartridge_id(const std::string &id);

[[nodiscard]] std::vector<ValidationIssue> validate_world(const WorldState &world);

// Load then validate; load failures come back as a single error issue.
[[nodiscard]] std::vector<ValidationIssue> validate_package(const std::filesystem::path &dir);

[[nodiscard]] bool has_errors(const std::vector<ValidationIssue> &issues);

} // namespace chronicle
