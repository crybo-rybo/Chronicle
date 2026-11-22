#include "chronicle/cli.hpp"

#include <cstdlib>
#include <iostream>

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/cartridge/validator.hpp"
#include "chronicle/game/cartridge_game.hpp"
#include "chronicle/library.hpp"
#include "chronicle/paths.hpp"
#include "chronicle/runtime.hpp"
#include "chronicle/version.hpp"

namespace chronicle {

namespace {

constexpr const char *USAGE = R"(Usage:
  chronicle [--scenario PATH | --tiny] [PLAY OPTIONS]
  chronicle run <id> [PLAY OPTIONS]
  chronicle validate --scenario PATH
  chronicle inspect --scenario PATH
  chronicle list
  chronicle install <path>
  chronicle pack --scenario PATH --output PATH

  chronicle --version

Play options:
  --base-url URL    OpenAI-compatible endpoint (or CHRONICLE_BASE_URL)
  --model NAME      Model to ask for (or CHRONICLE_MODEL)
  --save-dir PATH   Where saves are kept (or CHRONICLE_SAVE_DIR);
                    default ~/.chronicle/saves

Run a cartridge (default) or a subcommand. Without a model endpoint, NPC
dialogue uses a deterministic stub so mechanics remain playable.)";

bool is_subcommand(const std::string &word) {
    return word == "run" || word == "validate" || word == "inspect" || word == "list" ||
           word == "install" || word == "pack";
}

std::optional<std::string> semantic_error(const CliArgs &args) {
    if (args.help || args.version) {
        return std::nullopt;
    }
    const auto rejects_play_options = [&]() -> std::optional<std::string> {
        if (args.tiny || args.base_url || args.model || args.save_dir) {
            return args.command + " does not accept play options";
        }
        return std::nullopt;
    };

    if (args.command == "play") {
        if (!args.positional.empty() || args.output) {
            return "play accepts only --scenario, --tiny, and play options";
        }
        if (args.tiny && args.scenario) {
            return "--tiny and --scenario are mutually exclusive";
        }
        return std::nullopt;
    }
    if (args.command == "run") {
        if (args.positional.size() != 1) {
            return "run requires exactly one cartridge id";
        }
        if (args.scenario || args.output || args.tiny) {
            return "run accepts one cartridge id and play options";
        }
        return std::nullopt;
    }
    if (args.command == "validate" || args.command == "inspect") {
        if (args.positional.size() != 0 || !args.scenario || args.output) {
            return args.command + " requires exactly one --scenario";
        }
        return rejects_play_options();
    }
    if (args.command == "list") {
        if (!args.positional.empty() || args.scenario || args.output) {
            return "list does not accept arguments";
        }
        return rejects_play_options();
    }
    if (args.command == "install") {
        if (args.positional.size() != 1 || args.scenario || args.output) {
            return "install requires exactly one path";
        }
        return rejects_play_options();
    }
    if (args.command == "pack") {
        if (!args.positional.empty() || !args.scenario || !args.output) {
            return "pack requires exactly one --scenario and one --output";
        }
        return rejects_play_options();
    }
    return "Unknown command: " + args.command;
}

int print_issues_and_status(const std::vector<ValidationIssue> &issues) {
    for (const auto &issue : issues) {
        if (issue.level == IssueLevel::warning) {
            std::cout << issue.to_string() << '\n';
        }
    }
    bool any_error = false;
    for (const auto &issue : issues) {
        if (issue.level == IssueLevel::error) {
            std::cout << issue.to_string() << '\n';
            any_error = true;
        }
    }
    if (any_error) {
        std::cout << "INVALID\n";
        return 1;
    }
    std::cout << "OK\n";
    return 0;
}

// The cartridge's world, or nothing after printing why it cannot be played.
std::optional<WorldState> load_playable(const std::filesystem::path &package) {
    std::vector<ValidationIssue> issues;
    try {
        WorldState world = load_package(package);
        issues = validate_world(world);
        if (!has_errors(issues)) {
            return world;
        }
    } catch (const std::exception &exc) {
        issues = {{.message = exc.what(), .level = IssueLevel::error}};
    }
    for (const auto &issue : issues) {
        if (issue.level == IssueLevel::error) {
            std::cerr << issue.to_string() << '\n';
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> save_root(const CliArgs &args) {
    if (args.save_dir) {
        return *args.save_dir;
    }
    const char *from_environment = std::getenv("CHRONICLE_SAVE_DIR");
    if (from_environment != nullptr && *from_environment != '\0') {
        return from_environment;
    }
    return std::nullopt;
}

int play(WorldState world, const CliArgs &args) {
    const auto saves = save_directory(world.manifest.id, save_root(args));
    CartridgeGame game(std::move(world), saves);
    ConsoleRuntime runtime(game, resolve_endpoint(args.base_url, args.model, &game.world().config));
    return runtime.run();
}

int play_package(const std::filesystem::path &package, const CliArgs &args) {
    auto world = load_playable(package);
    return world ? play(std::move(*world), args) : 1;
}

} // namespace

CliArgs parse_cli(const std::vector<std::string> &args) {
    CliArgs parsed;
    std::size_t index = 0;

    const auto take_value = [&](const std::string &flag, std::optional<std::string> &out) -> bool {
        if (args[index] == flag) {
            if (out) {
                parsed.error = flag + " may be specified only once";
                return true;
            }
            if (index + 1 >= args.size() || args[index + 1].starts_with("--")) {
                parsed.error = flag + " requires a value";
                return true;
            }
            out = args[++index];
            return true;
        }
        if (args[index].starts_with(flag + "=")) {
            if (out) {
                parsed.error = flag + " may be specified only once";
                return true;
            }
            out = args[index].substr(flag.size() + 1);
            if (out->empty()) {
                parsed.error = flag + " requires a value";
            }
            return true;
        }
        return false;
    };

    bool command_seen = false;
    for (; index < args.size(); ++index) {
        const std::string &arg = args[index];
        if (parsed.error) {
            break;
        }
        if (take_value("--scenario", parsed.scenario) ||
            take_value("--base-url", parsed.base_url) || take_value("--model", parsed.model) ||
            take_value("--output", parsed.output) || take_value("--save-dir", parsed.save_dir)) {
            continue;
        }
        if (arg == "--tiny") {
            parsed.tiny = true;
        } else if (arg == "--version") {
            parsed.version = true;
        } else if (arg == "--help" || arg == "-h") {
            parsed.help = true;
        } else if (!command_seen && is_subcommand(arg)) {
            parsed.command = arg;
            command_seen = true;
        } else if (!arg.starts_with("-")) {
            parsed.positional.push_back(arg);
        } else {
            parsed.error = "Unknown option: " + arg;
        }
    }
    if (!parsed.error) {
        parsed.error = semantic_error(parsed);
    }
    return parsed;
}

WorldState build_tiny_world() {
    using nlohmann::json;
    const json manifest{
        {"id", "tiny"},
        {"name", "Tiny Room"},
        {"version", "1.0.0"},
        {"chronicle_schema_version", SCHEMA_VERSION},
        {"files",
         {{"config", "config.json"},
          {"world", "world.json"},
          {"npcs", "npcs.json"},
          {"facts", "facts.json"},
          {"flags", "flags.json"},
          {"events", "events.json"}}},
        {"metadata", {{"description", "Built-in harness smoke demo."}}},
    };
    const json world{
        {"start_location", "room"},
        {"locations",
         {{"room",
           {{"name", "Bare Room"},
            {"base_description", "You stand in a bare room. A quiet stranger waits here. "
                                 "Type 'talk stranger' to speak, or 'quit' to leave."},
            {"exits", json::object()},
            {"items", json::array()}}}}},
        {"items", json::object()},
    };
    const json npcs{
        {"npcs",
         {{"stranger",
           {{"identity",
             {{"name", "Stranger"},
              {"role", "a quiet stranger"},
              {"personality_summary", "Reserved and watchful."},
              {"backstory", "No one knows how long they have waited here."},
              {"goals", json::array({"Observe the visitor"})},
              {"tool_policy", {{"allowed_tools", json::array({"say"})}}}}},
            {"state", {{"current_location", "room"}}}}}}},
    };
    const json empty = json::object();
    return assemble_world(manifest, empty, world, npcs, empty, empty, empty);
}

int run_cli(int argc, char **argv) {
    std::vector<std::string> raw;
    for (int i = 1; i < argc; ++i) {
        raw.emplace_back(argv[i]);
    }
    const CliArgs args = parse_cli(raw);

    if (args.error) {
        std::cerr << *args.error << '\n' << USAGE << '\n';
        return 2;
    }
    if (args.help) {
        std::cout << USAGE << '\n';
        return 0;
    }
    if (args.version) {
        std::cout << "chronicle " << CHRONICLE_VERSION << '\n';
        return 0;
    }

    try {
        if (args.command == "play") {
            return args.tiny ? play(build_tiny_world(), args)
                             : play_package(resolve_scenario(args.scenario), args);
        }
        if (args.command == "run") {
            if (args.positional.empty()) {
                std::cerr << "run requires a cartridge id\n";
                return 2;
            }
            const auto &id = args.positional.front();
            if (!is_safe_cartridge_id(id)) {
                std::cerr << "Invalid cartridge id: " << id << '\n';
                return 1;
            }
            const auto path = default_library_dir() / id;
            if (!std::filesystem::is_directory(path) || std::filesystem::is_symlink(path)) {
                std::cerr << "Not installed: " << args.positional.front() << '\n';
                return 1;
            }
            return play_package(path, args);
        }
        if (args.command == "validate") {
            if (!args.scenario) {
                std::cerr << "validate requires --scenario\n";
                return 2;
            }
            return print_issues_and_status(validate_package(*args.scenario));
        }
        if (args.command == "inspect") {
            if (!args.scenario) {
                std::cerr << "inspect requires --scenario\n";
                return 2;
            }
            const auto info = inspect_package(std::filesystem::absolute(*args.scenario));
            std::cout << "id:      " << info.id << '\n';
            std::cout << "name:    " << info.name << '\n';
            std::cout << "version: " << info.version << '\n';
            std::cout << "schema:  " << info.schema << '\n';
            std::cout << "path:    " << info.path << '\n';
            std::cout << "ready:   " << (info.ready ? "true" : "false") << '\n';
            for (const auto &line : info.errors) {
                std::cout << line << '\n';
            }
            for (const auto &line : info.warnings) {
                std::cout << line << '\n';
            }
            return info.ready ? 0 : 1;
        }
        if (args.command == "list") {
            const auto items = list_cartridges();
            if (items.empty()) {
                std::cout << "No cartridges installed.\n";
                return 0;
            }
            for (const auto &item : items) {
                std::cout << item.id << '\t' << item.name << "\tv" << item.version << '\t'
                          << item.path << '\n';
            }
            return 0;
        }
        if (args.command == "install") {
            if (args.positional.empty()) {
                std::cerr << "install requires a path\n";
                return 2;
            }
            const auto dest = install_cartridge(args.positional.front());
            std::cout << "Installed to " << dest.string() << '\n';
            return 0;
        }
        if (args.command == "pack") {
            if (!args.scenario || !args.output) {
                std::cerr << "pack requires --scenario and --output\n";
                return 2;
            }
            const auto out = pack_cartridge(*args.scenario, *args.output);
            std::cout << "Wrote " << out.string() << '\n';
            return 0;
        }
    } catch (const std::exception &exc) {
        std::cerr << exc.what() << '\n';
        return 1;
    }
    std::cerr << USAGE << '\n';
    return 2;
}

} // namespace chronicle
