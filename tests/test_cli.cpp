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

TEST(CliTest, ParsesRunCartridgeId) {
    auto result = parse_cli_args({"run", "demo_game"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Run);
    ASSERT_TRUE(options.cartridge_id.has_value());
    EXPECT_EQ(*options.cartridge_id, "demo_game");
}

TEST(CliTest, ParsesInstallSource) {
    auto result = parse_cli_args({"install", "/tmp/demo.chronicle", "--library", "/tmp/lib"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Install);
    EXPECT_EQ(options.install_source, "/tmp/demo.chronicle");
    ASSERT_TRUE(options.library_root.has_value());
    EXPECT_EQ(*options.library_root, "/tmp/lib");
}

TEST(CliTest, ParsesPackScenario) {
    auto result = parse_cli_args({"pack", "--scenario", "examples/minimal_scenario"});

    ASSERT_TRUE(std::holds_alternative<CliOptions>(result));
    const auto &options = std::get<CliOptions>(result);
    EXPECT_EQ(options.mode, CliMode::Pack);
    EXPECT_EQ(options.scenario_dir, "examples/minimal_scenario");
}

TEST(CliTest, UsageIncludesLibraryCommands) {
    EXPECT_NE(cli_usage().find("chronicle list"), std::string::npos);
    EXPECT_NE(cli_usage().find("chronicle run <cartridge-id>"), std::string::npos);
    EXPECT_NE(cli_usage().find("chronicle install"), std::string::npos);
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
