/**
 * @file logger.cpp
 * @brief Implementation of Chronicle diagnostic logging.
 */

#include "diagnostics/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace chronicle::logging {
namespace {

std::mutex &logger_mutex() {
    static std::mutex mutex;
    return mutex;
}

Settings &logger_settings() {
    static Settings settings{
        .enabled = CHRONICLE_ENABLE_LOGGING != 0,
        .min_level = Level::Debug,
        .sink = &std::clog,
    };
    return settings;
}

std::unique_ptr<std::ofstream> &owned_log_file() {
    static std::unique_ptr<std::ofstream> file;
    return file;
}

bool should_write(const Settings &settings, Level level) {
    return settings.enabled && static_cast<int>(level) >= static_cast<int>(settings.min_level);
}

std::string lowercase(std::string_view text) {
    std::string copy(text);
    std::ranges::transform(copy, copy.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return copy;
}

bool is_truthy(std::string_view text) {
    auto value = lowercase(text);
    return value == "1" || value == "true" || value == "on" || value == "yes";
}

bool is_falsy(std::string_view text) {
    auto value = lowercase(text);
    return value == "0" || value == "false" || value == "off" || value == "no";
}

} // namespace

void configure(Settings settings) {
    if (!settings.sink) {
        settings.sink = &std::clog;
    }

    std::scoped_lock lock(logger_mutex());
    logger_settings() = settings;
}

void reset() {
    {
        std::scoped_lock lock(logger_mutex());
        owned_log_file().reset();
    }
    configure({
        .enabled = CHRONICLE_ENABLE_LOGGING != 0,
        .min_level = Level::Debug,
        .sink = &std::clog,
    });
}

void configure_from_environment() {
    Settings settings{
        .enabled = CHRONICLE_ENABLE_LOGGING != 0,
        .min_level = Level::Debug,
        .sink = &std::clog,
    };

    if (const char *value = std::getenv("CHRONICLE_LOG")) {
        if (auto parsed = parse_level(value)) {
            settings.enabled = true;
            settings.min_level = *parsed;
        } else if (is_truthy(value)) {
            settings.enabled = true;
        } else if (is_falsy(value)) {
            settings.enabled = false;
        }
    }

    if (const char *value = std::getenv("CHRONICLE_LOG_LEVEL")) {
        if (auto parsed = parse_level(value)) {
            settings.min_level = *parsed;
        }
    }

    if (const char *path = std::getenv("CHRONICLE_LOG_FILE"); path && path[0] != '\0') {
        auto file = std::make_unique<std::ofstream>(path, std::ios::app);
        if (*file) {
            settings.sink = file.get();
            std::scoped_lock lock(logger_mutex());
            owned_log_file() = std::move(file);
        } else {
            std::cerr << "Chronicle logging: failed to open CHRONICLE_LOG_FILE='" << path
                      << "'; falling back to stderr\n";
        }
    }

    configure(settings);
    logging::write(logging::Level::Info, "logging",
                   "configured enabled=" + std::string(settings.enabled ? "true" : "false") +
                       " min_level=" + std::string(to_string(settings.min_level)));
}

bool is_enabled() {
    std::scoped_lock lock(logger_mutex());
    return logger_settings().enabled;
}

Level min_level() {
    std::scoped_lock lock(logger_mutex());
    return logger_settings().min_level;
}

std::string_view to_string(Level level) {
    switch (level) {
    case Level::Debug:
        return "debug";
    case Level::Info:
        return "info";
    case Level::Warning:
        return "warning";
    case Level::Error:
        return "error";
    }
    return "unknown";
}

std::optional<Level> parse_level(std::string_view text) {
    auto value = lowercase(text);
    if (value == "debug") {
        return Level::Debug;
    }
    if (value == "info") {
        return Level::Info;
    }
    if (value == "warning" || value == "warn") {
        return Level::Warning;
    }
    if (value == "error") {
        return Level::Error;
    }
    return std::nullopt;
}

void write(Level level, std::string_view category, std::string_view message) {
    std::scoped_lock lock(logger_mutex());
    auto &settings = logger_settings();
    if (!should_write(settings, level)) {
        return;
    }

    *settings.sink << "[chronicle:" << to_string(level) << "][" << category << "] " << message
                   << '\n';
}

} // namespace chronicle::logging
