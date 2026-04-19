#include "diagnostics/logger.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace chronicle;

TEST(LoggerTest, DisabledLoggerDoesNotWrite) {
    std::ostringstream sink;
    logging::configure({.enabled = false, .min_level = logging::Level::Debug, .sink = &sink});

    logging::write(logging::Level::Info, "ai", "starting dialogue");

    EXPECT_TRUE(sink.str().empty());
    logging::reset();
}

TEST(LoggerTest, EnabledLoggerWritesCategorizedMessages) {
    std::ostringstream sink;
    logging::configure({.enabled = true, .min_level = logging::Level::Debug, .sink = &sink});

    logging::write(logging::Level::Info, "ai", "starting dialogue");

    EXPECT_EQ(sink.str(), "[chronicle:info][ai] starting dialogue\n");
    logging::reset();
}

TEST(LoggerTest, LoggerFiltersBelowMinimumLevel) {
    std::ostringstream sink;
    logging::configure({.enabled = true, .min_level = logging::Level::Warning, .sink = &sink});

    logging::write(logging::Level::Debug, "tools", "hidden detail");
    logging::write(logging::Level::Error, "tools", "visible failure");

    EXPECT_EQ(sink.str(), "[chronicle:error][tools] visible failure\n");
    logging::reset();
}

TEST(LoggerTest, ParseLevelAcceptsKnownLevelNames) {
    EXPECT_EQ(logging::parse_level("debug"), logging::Level::Debug);
    EXPECT_EQ(logging::parse_level("INFO"), logging::Level::Info);
    EXPECT_EQ(logging::parse_level("warning"), logging::Level::Warning);
    EXPECT_EQ(logging::parse_level("error"), logging::Level::Error);
    EXPECT_EQ(logging::parse_level("nope"), std::nullopt);
}
