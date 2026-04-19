/**
 * @file logger.hpp
 * @brief Lightweight opt-in diagnostic logging for Chronicle-owned code.
 */

#pragma once

#include <iosfwd>
#include <optional>
#include <string_view>

namespace chronicle::logging {

/// @brief Diagnostic severity, ordered from most verbose to most severe.
enum class Level {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

/// @brief Runtime logger settings.
struct Settings {
    bool enabled = false;
    Level min_level = Level::Debug;
    std::ostream *sink = nullptr;
};

/// @brief Configure Chronicle logging for the current process.
void configure(Settings settings);

/// @brief Restore the build-default logger configuration.
void reset();

/// @brief Apply CHRONICLE_LOG, CHRONICLE_LOG_LEVEL, and CHRONICLE_LOG_FILE overrides.
void configure_from_environment();

/// @return Whether diagnostic logging is currently enabled.
bool is_enabled();

/// @return The current minimum severity.
Level min_level();

/// @brief Convert a level to the lowercase text used in log output.
std::string_view to_string(Level level);

/// @brief Parse a case-insensitive level name.
std::optional<Level> parse_level(std::string_view text);

/// @brief Write a categorized diagnostic message if logging is enabled for @p level.
void write(Level level, std::string_view category, std::string_view message);

} // namespace chronicle::logging
