#include "engine/command_parser.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace chronicle;
using Phase = GamePhase;

namespace {

std::filesystem::path write_command_config(std::string_view contents) {
    auto path = std::filesystem::temp_directory_path() / "chronicle_command_parser_config.json";
    std::ofstream out(path);
    out << contents;
    return path;
}

CommandParser fallback_parser() {
    return CommandParser("/nonexistent/chronicle/scenario/config.json");
}

} // namespace

// ---------------------------------------------------------------------------
// Basic verbs
// ---------------------------------------------------------------------------

TEST(CommandParserTest, GoNorth) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("go north", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, Look) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("look", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
    EXPECT_EQ(cmd.primary_arg, "");
}

TEST(CommandParserTest, ExamineKey) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("examine key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Examine);
    EXPECT_EQ(cmd.primary_arg, "key");
}

TEST(CommandParserTest, TakeSword) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("take sword", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Take);
    EXPECT_EQ(cmd.primary_arg, "sword");
}

TEST(CommandParserTest, DropShield) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("drop shield", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Drop);
    EXPECT_EQ(cmd.primary_arg, "shield");
}

TEST(CommandParserTest, UsePotion) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("use potion", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "potion");
    EXPECT_EQ(cmd.secondary_arg, "");
}

TEST(CommandParserTest, GiveCoin) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("give coin", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Give);
    EXPECT_EQ(cmd.primary_arg, "coin");
}

TEST(CommandParserTest, TalkMarcus) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("talk marcus", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Talk);
    EXPECT_EQ(cmd.primary_arg, "marcus");
}

TEST(CommandParserTest, InventoryCommand) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("inventory", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, SaveCommand) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("save", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Save);
}

TEST(CommandParserTest, LoadCommand) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("load", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Load);
}

TEST(CommandParserTest, QuitCommand) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("quit", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, HelpCommand) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("help", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Help);
}

// ---------------------------------------------------------------------------
// Aliases
// ---------------------------------------------------------------------------

TEST(CommandParserTest, AliasWalk) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("walk north", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, AliasL) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("l", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

TEST(CommandParserTest, AliasX) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("x key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Examine);
    EXPECT_EQ(cmd.primary_arg, "key");
}

TEST(CommandParserTest, AliasGet) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("get sword", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Take);
    EXPECT_EQ(cmd.primary_arg, "sword");
}

TEST(CommandParserTest, AliasSpeakTo) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("speak marcus", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Talk);
    EXPECT_EQ(cmd.primary_arg, "marcus");
}

TEST(CommandParserTest, AliasI) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("i", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, AliasExit) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("exit", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, AliasQuestionMark) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("?", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Help);
}

TEST(CommandParserTest, LoadsAliasesFromConfigFile) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "bag": "inventory"
        }
    })");

    CommandParser parser(config_path);
    auto cmd = parser.parse("bag", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);

    std::filesystem::remove(config_path);
}

TEST(CommandParserTest, LoadsStringCommandAliasesFromConfigFile) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "dock": "go north",
            "kit": "inventory",
            "unlock hatch": "use brass key on hatch"
        }
    })");

    CommandParser parser(config_path);

    auto move = parser.parse("dock", Phase::Playing);
    EXPECT_EQ(move.verb, CommandVerb::Go);
    EXPECT_EQ(move.primary_arg, "north");
    EXPECT_EQ(move.raw_input, "dock");

    auto inventory = parser.parse("kit", Phase::Playing);
    EXPECT_EQ(inventory.verb, CommandVerb::Inventory);

    auto use = parser.parse("unlock hatch", Phase::Playing);
    EXPECT_EQ(use.verb, CommandVerb::Use);
    EXPECT_EQ(use.primary_arg, "brass key");
    EXPECT_EQ(use.secondary_arg, "hatch");

    std::filesystem::remove(config_path);
}

TEST(CommandParserTest, PartialConfigPreservesDefaults) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "bag": "inventory"
        }
    })");

    CommandParser parser(config_path);

    // Custom alias works
    auto cmd1 = parser.parse("bag", Phase::Playing);
    EXPECT_EQ(cmd1.verb, CommandVerb::Inventory);

    // Built-in canonical verb still works
    auto cmd2 = parser.parse("inventory", Phase::Playing);
    EXPECT_EQ(cmd2.verb, CommandVerb::Inventory);

    // Other default verbs preserved
    auto cmd3 = parser.parse("quit", Phase::Playing);
    EXPECT_EQ(cmd3.verb, CommandVerb::Quit);

    auto cmd4 = parser.parse("go north", Phase::Playing);
    EXPECT_EQ(cmd4.verb, CommandVerb::Go);

    auto cmd5 = parser.parse("look", Phase::Playing);
    EXPECT_EQ(cmd5.verb, CommandVerb::Look);

    auto cmd6 = parser.parse("n", Phase::Playing);
    EXPECT_EQ(cmd6.verb, CommandVerb::Go);

    std::filesystem::remove(config_path);
}

TEST(CommandParserTest, MissingConfigFallsBackToBuiltInAliases) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("i", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, InConversationConfigAliasCanBeHardCommand) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "bag": "inventory"
        }
    })");

    CommandParser parser(config_path);
    auto cmd = parser.parse("bag", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);

    std::filesystem::remove(config_path);
}

// ---------------------------------------------------------------------------
// Use syntax with prepositions
// ---------------------------------------------------------------------------

TEST(CommandParserTest, UseKeyOnDoor) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("use key on door", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "door");
}

TEST(CommandParserTest, UseKeyWithDoor) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("use key with door", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "door");
}

TEST(CommandParserTest, UseKeyNoSecondary) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("use key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "");
}

// ---------------------------------------------------------------------------
// InConversation phase
// ---------------------------------------------------------------------------

TEST(CommandParserTest, InConversationFreeText) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("tell me about the theft", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Dialogue);
    EXPECT_EQ(cmd.raw_input, "tell me about the theft");
}

TEST(CommandParserTest, InConversationQuitWorks) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("quit", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, InConversationInventoryWorks) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("inventory", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, InConversationLookWorks) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("look", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(CommandParserTest, EmptyInputUnknown) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
}

TEST(CommandParserTest, WhitespaceOnlyUnknown) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("   ", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
}

// ---------------------------------------------------------------------------
// Directional shortcuts
// ---------------------------------------------------------------------------

TEST(CommandParserTest, DirectionalShortcutN) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("n", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, DirectionalShortcutS) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("s", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "south");
}

TEST(CommandParserTest, DirectionalShortcutE) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("e", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "east");
}

TEST(CommandParserTest, DirectionalShortcutW) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("w", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "west");
}

// ---------------------------------------------------------------------------
// Case insensitivity
// ---------------------------------------------------------------------------

TEST(CommandParserTest, CaseInsensitive) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("LOOK", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

TEST(CommandParserTest, CaseInsensitiveMixed) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("Go North", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, GoDirectionNormalizesAllCaps) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("GO NORTH", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, DialogueRawInputPreserved) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("Tell me about the NORTH gate", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Dialogue);
    EXPECT_EQ(cmd.primary_arg, "Tell me about the NORTH gate");
}

// ---------------------------------------------------------------------------
// Raw input preservation
// ---------------------------------------------------------------------------

TEST(CommandParserTest, RawInputPreserved) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("go north", Phase::Playing);
    EXPECT_EQ(cmd.raw_input, "go north");
}

// ---------------------------------------------------------------------------
// Unknown verb
// ---------------------------------------------------------------------------

TEST(CommandParserTest, UnknownVerb) {
    auto parser = fallback_parser();
    auto cmd = parser.parse("dance wildly", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
    EXPECT_EQ(cmd.primary_arg, "dance");
}
