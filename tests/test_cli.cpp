#include "engine/cli.hpp"
#include <gtest/gtest.h>

namespace chronicle {

TEST(CliTest, DefaultsToBundledDataScenario) {
    auto result = parse_cli_args({});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Run);
    EXPECT_EQ(options.scenario_dir, "data");
}

TEST(CliTest, ParsesRunScenarioPath) {
    auto result = parse_cli_args({"--scenario", "examples/market"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Run);
    EXPECT_EQ(options.scenario_dir, "examples/market");
}

TEST(CliTest, ParsesValidateScenarioPath) {
    auto result = parse_cli_args({"validate", "--scenario", "examples/market"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Validate);
    EXPECT_EQ(options.scenario_dir, "examples/market");
}

TEST(CliTest, ParsesInspectWithDefaultScenarioPath) {
    auto result = parse_cli_args({"inspect"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Inspect);
    EXPECT_EQ(options.scenario_dir, "data");
}

TEST(CliTest, ParsesInspectScenarioPath) {
    auto result = parse_cli_args({"inspect", "--scenario", "examples/minimal_scenario"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Inspect);
    EXPECT_EQ(options.scenario_dir, "examples/minimal_scenario");
}

TEST(CliTest, UsageIncludesInspectCommand) {
    EXPECT_NE(cli_usage().find("chronicle inspect --scenario <dir>"), std::string::npos);
}

TEST(CliTest, MissingScenarioArgumentReturnsError) {
    auto result = parse_cli_args({"--scenario"});

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("--scenario"), std::string::npos);
}

TEST(CliTest, UnknownArgumentReturnsError) {
    auto result = parse_cli_args({"--mystery"});

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("Unknown argument"), std::string::npos);
}

} // namespace chronicle
