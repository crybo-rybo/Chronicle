#include <gtest/gtest.h>

#include "chronicle/cartridge/validator.hpp"
#include "chronicle/cli.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

TEST(Cli, DefaultIsPlay) {
    const auto args = parse_cli({});
    EXPECT_EQ(args.command, "play");
    EXPECT_FALSE(args.error.has_value());
}

TEST(Cli, FlagsWithSpaceAndEqualsForms) {
    const auto args = parse_cli({"--scenario", "examples/minimal", "--model=qwen3:8b", "--base-url",
                                 "http://localhost:11434/v1"});
    EXPECT_EQ(args.scenario, "examples/minimal");
    EXPECT_EQ(args.model, "qwen3:8b");
    EXPECT_EQ(args.base_url, "http://localhost:11434/v1");
}

TEST(Cli, SubcommandsAndPositionals) {
    const auto run = parse_cli({"run", "minimal", "--model", "m"});
    EXPECT_EQ(run.command, "run");
    ASSERT_EQ(run.positional.size(), 1u);
    EXPECT_EQ(run.positional.front(), "minimal");

    const auto install = parse_cli({"install", "some/dir"});
    EXPECT_EQ(install.command, "install");
    EXPECT_EQ(install.positional.front(), "some/dir");

    const auto pack = parse_cli({"pack", "--scenario", "s", "--output", "o.chronicle"});
    EXPECT_EQ(pack.command, "pack");
    EXPECT_EQ(pack.scenario, "s");
    EXPECT_EQ(pack.output, "o.chronicle");
}

TEST(Cli, BoolFlags) {
    const auto args = parse_cli({"--tiny", "--version", "--help"});
    EXPECT_TRUE(args.tiny);
    EXPECT_TRUE(args.version);
    EXPECT_TRUE(args.help);
}

TEST(Cli, MissingFlagValueIsError) {
    const auto args = parse_cli({"--scenario"});
    ASSERT_TRUE(args.error.has_value());
    EXPECT_NE(args.error->find("--scenario"), std::string::npos);
}

TEST(Cli, EmptyAndDuplicateFlagValuesAreErrors) {
    EXPECT_TRUE(parse_cli({"--model="}).error.has_value());
    EXPECT_TRUE(parse_cli({"--model", "first", "--model", "second"}).error.has_value());
    EXPECT_TRUE(parse_cli({"--scenario", "--tiny"}).error.has_value());
}

TEST(Cli, UnknownOptionIsError) {
    const auto args = parse_cli({"--frobnicate"});
    ASSERT_TRUE(args.error.has_value());
}

TEST(Cli, SubcommandNameAfterCommandIsPositional) {
    const auto args = parse_cli({"install", "list"});
    EXPECT_EQ(args.command, "install");
    ASSERT_EQ(args.positional.size(), 1u);
    EXPECT_EQ(args.positional.front(), "list");
}

TEST(Cli, CommandShapesAndOptionDomainsAreStrict) {
    const std::vector<std::vector<std::string>> invalid_commands{
        {"run"},
        {"run", "one", "two"},
        {"list", "extra"},
        {"list", "--model", "m"},
        {"validate", "--scenario", "s", "extra"},
        {"install", "one", "two"},
        {"pack", "--scenario", "s"},
        {"--tiny", "--scenario", "s"},
    };
    for (const auto &invalid : invalid_commands) {
        EXPECT_TRUE(parse_cli(invalid).error.has_value());
    }
}

TEST(Cli, TinyWorldIsValid) {
    const WorldState world = build_tiny_world();
    EXPECT_FALSE(has_errors(validate_world(world)));
    EXPECT_EQ(world.manifest.id, "tiny");
    EXPECT_TRUE(world.npcs.contains("stranger"));
    const auto &allowed = world.npcs.at("stranger").identity.tool_policy.allowed_tools;
    EXPECT_EQ(allowed, std::vector<std::string>{"say"});
}

TEST(Cli, InspectReturnsFailureForInvalidCartridge) {
    testing::TempDir package("cli-inspect-invalid");
    testing::write_package(package.path(), {{"scenario", {{"chronicle_schema_version", 99}}}});
    std::vector<std::string> storage{"chronicle", "inspect", "--scenario", package.path().string()};
    std::vector<char *> argv;
    argv.reserve(storage.size());
    for (auto &arg : storage) {
        argv.push_back(arg.data());
    }

    ::testing::internal::CaptureStdout();
    const int result = run_cli(static_cast<int>(argv.size()), argv.data());
    const std::string output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(result, 1);
    EXPECT_NE(output.find("ready:   false"), std::string::npos);
}

} // namespace
} // namespace chronicle
