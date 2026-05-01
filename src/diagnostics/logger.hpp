/**
 * @file logger.hpp
 * @brief Lightweight opt-in diagnostic logging for Chronicle-owned code.
 *
 * @details Logging is process-global and guarded internally for concurrent
 * calls.  It is intended for diagnostics around loading, AI setup, dialogue,
 * and mutation processing without exposing hidden scenario data by default.
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
    /// Whether calls to @ref write should emit messages.
    bool enabled = false;

    /// Minimum severity that will be emitted when logging is enabled.
    Level min_level = Level::Debug;

    /// Borrowed output stream.  The caller must keep it alive while configured.
    std::ostream *sink = nullptr;
};

/// @brief Configure Chronicle logging for the current process.
///
/// @details Thread-safe.  The sink pointer is borrowed, not owned.
void configure(Settings settings);

/// @brief Restore the build-default logger configuration.
void reset();

/// @brief Apply CHRONICLE_LOG, CHRONICLE_LOG_LEVEL, and CHRONICLE_LOG_FILE overrides.
///
/// @details @c CHRONICLE_LOG enables logging when set to a truthy value or a
/// level name, @c CHRONICLE_LOG_LEVEL accepts @c debug, @c info, @c warning,
/// @c warn, or @c error, and @c CHRONICLE_LOG_FILE redirects output to the
/// named file.
void configure_from_environment();

/// @return Whether diagnostic logging is currently enabled.
bool is_enabled();

/// @return The current minimum severity.
Level min_level();

/// @brief Convert a level to the lowercase text used in log output.
std::string_view to_string(Level level);

/// @brief Parse a case-insensitive level name.
///
/// @param text Input such as @c "debug", @c "info", @c "warning", or @c "error".
/// @return The matching severity, or @c std::nullopt if unrecognized.
std::optional<Level> parse_level(std::string_view text);

/// @brief Write a categorized diagnostic message if logging is enabled for @p level.
///
/// @details Thread-safe.  If logging is disabled or @p level is below the
/// configured threshold, this is a no-op.  Formatting is deliberately simple:
/// level, category, and message.
void write(Level level, std::string_view category, std::string_view message);

} // namespace chronicle::logging
