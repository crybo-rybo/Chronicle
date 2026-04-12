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

} // namespace

// ---------------------------------------------------------------------------
// Basic verbs
// ---------------------------------------------------------------------------

TEST(CommandParserTest, GoNorth) {
    CommandParser parser;
    auto cmd = parser.parse("go north", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, Look) {
    CommandParser parser;
    auto cmd = parser.parse("look", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
    EXPECT_EQ(cmd.primary_arg, "");
}

TEST(CommandParserTest, ExamineKey) {
    CommandParser parser;
    auto cmd = parser.parse("examine key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Examine);
    EXPECT_EQ(cmd.primary_arg, "key");
}

TEST(CommandParserTest, TakeSword) {
    CommandParser parser;
    auto cmd = parser.parse("take sword", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Take);
    EXPECT_EQ(cmd.primary_arg, "sword");
}

TEST(CommandParserTest, DropShield) {
    CommandParser parser;
    auto cmd = parser.parse("drop shield", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Drop);
    EXPECT_EQ(cmd.primary_arg, "shield");
}

TEST(CommandParserTest, UsePotion) {
    CommandParser parser;
    auto cmd = parser.parse("use potion", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "potion");
    EXPECT_EQ(cmd.secondary_arg, "");
}

TEST(CommandParserTest, GiveCoin) {
    CommandParser parser;
    auto cmd = parser.parse("give coin", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Give);
    EXPECT_EQ(cmd.primary_arg, "coin");
}

TEST(CommandParserTest, TalkMarcus) {
    CommandParser parser;
    auto cmd = parser.parse("talk marcus", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Talk);
    EXPECT_EQ(cmd.primary_arg, "marcus");
}

TEST(CommandParserTest, InventoryCommand) {
    CommandParser parser;
    auto cmd = parser.parse("inventory", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, SaveCommand) {
    CommandParser parser;
    auto cmd = parser.parse("save", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Save);
}

TEST(CommandParserTest, LoadCommand) {
    CommandParser parser;
    auto cmd = parser.parse("load", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Load);
}

TEST(CommandParserTest, QuitCommand) {
    CommandParser parser;
    auto cmd = parser.parse("quit", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, HelpCommand) {
    CommandParser parser;
    auto cmd = parser.parse("help", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Help);
}

// ---------------------------------------------------------------------------
// Aliases
// ---------------------------------------------------------------------------

TEST(CommandParserTest, AliasWalk) {
    CommandParser parser;
    auto cmd = parser.parse("walk north", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, AliasL) {
    CommandParser parser;
    auto cmd = parser.parse("l", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

TEST(CommandParserTest, AliasX) {
    CommandParser parser;
    auto cmd = parser.parse("x key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Examine);
    EXPECT_EQ(cmd.primary_arg, "key");
}

TEST(CommandParserTest, AliasGet) {
    CommandParser parser;
    auto cmd = parser.parse("get sword", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Take);
    EXPECT_EQ(cmd.primary_arg, "sword");
}

TEST(CommandParserTest, AliasSpeakTo) {
    CommandParser parser;
    auto cmd = parser.parse("speak marcus", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Talk);
    EXPECT_EQ(cmd.primary_arg, "marcus");
}

TEST(CommandParserTest, AliasI) {
    CommandParser parser;
    auto cmd = parser.parse("i", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, AliasExit) {
    CommandParser parser;
    auto cmd = parser.parse("exit", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, AliasQuestionMark) {
    CommandParser parser;
    auto cmd = parser.parse("?", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Help);
}

TEST(CommandParserTest, LoadsAliasesFromConfigFile) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "inventory": ["bag"]
        }
    })");

    CommandParser parser(config_path);
    auto cmd = parser.parse("bag", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);

    std::filesystem::remove(config_path);
}

TEST(CommandParserTest, MissingConfigFallsBackToBuiltInAliases) {
    CommandParser parser("/nonexistent/chronicle/config/default.json");
    auto cmd = parser.parse("i", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, InConversationConfigAliasCanBeHardCommand) {
    auto config_path = write_command_config(R"({
        "verb_aliases": {
            "inventory": ["bag"]
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
    CommandParser parser;
    auto cmd = parser.parse("use key on door", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "door");
}

TEST(CommandParserTest, UseKeyWithDoor) {
    CommandParser parser;
    auto cmd = parser.parse("use key with door", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "door");
}

TEST(CommandParserTest, UseKeyNoSecondary) {
    CommandParser parser;
    auto cmd = parser.parse("use key", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Use);
    EXPECT_EQ(cmd.primary_arg, "key");
    EXPECT_EQ(cmd.secondary_arg, "");
}

// ---------------------------------------------------------------------------
// InConversation phase
// ---------------------------------------------------------------------------

TEST(CommandParserTest, InConversationFreeText) {
    CommandParser parser;
    auto cmd = parser.parse("tell me about the theft", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Dialogue);
    EXPECT_EQ(cmd.raw_input, "tell me about the theft");
}

TEST(CommandParserTest, InConversationQuitWorks) {
    CommandParser parser;
    auto cmd = parser.parse("quit", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Quit);
}

TEST(CommandParserTest, InConversationInventoryWorks) {
    CommandParser parser;
    auto cmd = parser.parse("inventory", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Inventory);
}

TEST(CommandParserTest, InConversationLookWorks) {
    CommandParser parser;
    auto cmd = parser.parse("look", Phase::InConversation);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(CommandParserTest, EmptyInputUnknown) {
    CommandParser parser;
    auto cmd = parser.parse("", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
}

TEST(CommandParserTest, WhitespaceOnlyUnknown) {
    CommandParser parser;
    auto cmd = parser.parse("   ", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
}

// ---------------------------------------------------------------------------
// Directional shortcuts
// ---------------------------------------------------------------------------

TEST(CommandParserTest, DirectionalShortcutN) {
    CommandParser parser;
    auto cmd = parser.parse("n", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "north");
}

TEST(CommandParserTest, DirectionalShortcutS) {
    CommandParser parser;
    auto cmd = parser.parse("s", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "south");
}

TEST(CommandParserTest, DirectionalShortcutE) {
    CommandParser parser;
    auto cmd = parser.parse("e", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "east");
}

TEST(CommandParserTest, DirectionalShortcutW) {
    CommandParser parser;
    auto cmd = parser.parse("w", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "west");
}

// ---------------------------------------------------------------------------
// Case insensitivity
// ---------------------------------------------------------------------------

TEST(CommandParserTest, CaseInsensitive) {
    CommandParser parser;
    auto cmd = parser.parse("LOOK", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Look);
}

TEST(CommandParserTest, CaseInsensitiveMixed) {
    CommandParser parser;
    auto cmd = parser.parse("Go North", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Go);
    EXPECT_EQ(cmd.primary_arg, "North");
}

// ---------------------------------------------------------------------------
// Raw input preservation
// ---------------------------------------------------------------------------

TEST(CommandParserTest, RawInputPreserved) {
    CommandParser parser;
    auto cmd = parser.parse("go north", Phase::Playing);
    EXPECT_EQ(cmd.raw_input, "go north");
}

// ---------------------------------------------------------------------------
// Unknown verb
// ---------------------------------------------------------------------------

TEST(CommandParserTest, UnknownVerb) {
    CommandParser parser;
    auto cmd = parser.parse("dance wildly", Phase::Playing);
    EXPECT_EQ(cmd.verb, CommandVerb::Unknown);
    EXPECT_EQ(cmd.primary_arg, "dance");
}
