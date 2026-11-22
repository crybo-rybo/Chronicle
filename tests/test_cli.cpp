#include <array>
#include <cstdlib>
#include <gtest/gtest.h>
#include <map>

#include "chronicle/cartridge/validator.hpp"
#include "chronicle/cli.hpp"
#include "chronicle/llm/npc_sessions.hpp"
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

TEST(Cli, SaveDirIsAPlayOption) {
    EXPECT_EQ(parse_cli({"--save-dir", "d"}).save_dir, "d");
    EXPECT_FALSE(parse_cli({"run", "minimal", "--save-dir=d"}).error.has_value());
    EXPECT_TRUE(parse_cli({"--save-dir="}).error.has_value());
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
        {"list", "--save-dir", "d"},
        {"validate", "--scenario", "s", "--save-dir", "d"},
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

TEST(Cli, EmptyValuesAreErrorsInEitherForm) {
    for (const std::string flag : {"--scenario", "--model", "--base-url", "--save-dir"}) {
        EXPECT_EQ(parse_cli({flag, ""}).error, flag + " requires a value");
        EXPECT_EQ(parse_cli({flag + "="}).error, flag + " requires a value");
    }
}

TEST(Cli, AMistypedCommandIsNamed) {
    EXPECT_EQ(parse_cli({"valdiate", "--scenario", "s"}).error, "Unknown command: valdiate");
}

// The environment names an endpoint, and nothing else leaks in from the host.
class EndpointTest : public ::testing::Test {
  protected:
    EndpointTest() {
        for (const auto *name : NAMES) {
            if (const char *value = std::getenv(name)) {
                saved_[name] = value;
            }
            (void)::unsetenv(name);
        }
    }
    ~EndpointTest() override {
        for (const auto *name : NAMES) {
            (void)::unsetenv(name);
        }
        for (const auto &[name, value] : saved_) {
            (void)::setenv(name.c_str(), value.c_str(), 1);
        }
    }
    static std::string problem(const std::optional<std::string> &url,
                               const std::optional<std::string> &model) {
        const auto endpoint = resolve_endpoint(url, model, nullptr);
        return endpoint ? "" : endpoint.error();
    }

    static constexpr std::array NAMES{"CHRONICLE_BASE_URL", "CHRONICLE_MODEL",
                                      "CHRONICLE_DIALECT",  "CHRONICLE_API_KEY",
                                      "OPENAI_API_KEY",     "CHRONICLE_DISABLE_REASONING"};
    std::map<std::string, std::string> saved_;
};

TEST_F(EndpointTest, NoneIsNotAProblem) {
    const auto endpoint = resolve_endpoint(std::nullopt, std::nullopt, nullptr);
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_FALSE(endpoint->has_value());
}

TEST_F(EndpointTest, HalfOfOneIs) {
    EXPECT_NE(problem(std::nullopt, "m").find("Model 'm' has no endpoint"), std::string::npos);
    EXPECT_NE(problem("http://localhost:1/v1", std::nullopt).find("has no model"),
              std::string::npos);
    (void)::setenv("CHRONICLE_MODEL", "m", 1);
    EXPECT_NE(problem(std::nullopt, std::nullopt).find("has no endpoint"), std::string::npos);
    EXPECT_EQ(problem("http://localhost:1/v1", std::nullopt), "");
}

TEST_F(EndpointTest, AnUnusableOneIsReportedUpFront) {
    EXPECT_NE(problem("not a url", "m").find("cannot be used"), std::string::npos);
    (void)::setenv("CHRONICLE_DIALECT", "Anthropic", 1);
    const auto endpoint = resolve_endpoint("http://localhost:1", "m", nullptr);
    ASSERT_TRUE(endpoint.has_value()) << endpoint.error();
    EXPECT_TRUE((*endpoint)->anthropic_dialect);
    (void)::setenv("CHRONICLE_DIALECT", "antropic", 1);
    EXPECT_NE(problem("http://localhost:1", "m").find("must be openai or anthropic"),
              std::string::npos);
}

TEST_F(EndpointTest, TheCartridgeTunesSamplingOnly) {
    ConfigData config;
    config.temperature = 0.25;
    config.max_response_tokens = 99;
    config.inference_timeout_ms = 1'234;
    const auto endpoint = resolve_endpoint("http://localhost:1/v1", "m", &config);
    ASSERT_TRUE(endpoint.has_value() && endpoint->has_value());
    EXPECT_EQ((*endpoint)->temperature, 0.25);
    EXPECT_EQ((*endpoint)->max_tokens, 99);
    EXPECT_EQ((*endpoint)->timeout_ms, 1'234);
    EXPECT_EQ((*endpoint)->api_key, "not-needed");
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
