#include "persistence/save_system.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace chronicle;

class SaveSystemTest : public ::testing::Test {
  protected:
    std::filesystem::path test_dir_;
    std::unique_ptr<SaveSystem> save_system_;

    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "chronicle_save_test";
        std::filesystem::remove_all(test_dir_);
        save_system_ = std::make_unique<SaveSystem>(test_dir_);
    }

    void TearDown() override { std::filesystem::remove_all(test_dir_); }

    World make_test_world() {
        World w;
        w.clock.day = 2;
        w.clock.period = TimePeriod::Evening;

        Location loc;
        loc.id = "tavern";
        loc.name = "The Broken Wheel Tavern";
        loc.base_description = "A smoky common room.";
        w.locations["tavern"] = loc;

        Item note;
        note.id = "crumpled_note";
        note.name = "crumpled note";
        note.description = "A note.";
        w.items["crumpled_note"] = note;

        w.player.current_location = "tavern";
        w.player.inventory = {"crumpled_note"};
        w.player.known_facts = {"fact1"};

        w.flags["intro_complete"] = true;
        w.total_turns_elapsed = 15;

        return w;
    }
};

TEST_F(SaveSystemTest, RoundtripWorld) {
    auto world = make_test_world();
    save_system_->save(world, 1);

    auto result = save_system_->load(1);
    ASSERT_TRUE(result.has_value());

    const auto &loaded = result->world;
    EXPECT_EQ(loaded.clock.day, 2);
    EXPECT_EQ(loaded.clock.period, TimePeriod::Evening);
    EXPECT_EQ(loaded.player.current_location, "tavern");
    EXPECT_EQ(loaded.player.inventory.size(), 1);
    EXPECT_EQ(loaded.player.inventory[0], "crumpled_note");
    EXPECT_EQ(loaded.flags.at("intro_complete"), true);
    EXPECT_EQ(loaded.total_turns_elapsed, 15);
    EXPECT_EQ(loaded.locations.at("tavern").name, "The Broken Wheel Tavern");
    EXPECT_EQ(loaded.items.at("crumpled_note").name, "crumpled note");
}

TEST_F(SaveSystemTest, LoadMissingFileReturnsNullopt) {
    auto result = save_system_->load(99);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SaveSystemTest, LoadCorruptedJsonReturnsNullopt) {
    std::filesystem::create_directories(test_dir_);
    std::ofstream out(test_dir_ / "slot_1.json");
    out << "this is not json at all!!!{{{";
    out.close();

    auto result = save_system_->load(1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SaveSystemTest, LoadWrongVersionReturnsNullopt) {
    std::filesystem::create_directories(test_dir_);
    nlohmann::json j;
    j["version"] = 999;
    j["metadata"] = {};
    j["world"] = nlohmann::json::object();

    std::ofstream out(test_dir_ / "slot_1.json");
    out << j.dump(2);
    out.close();

    auto result = save_system_->load(1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SaveSystemTest, ListSlotsReflectsFiles) {
    auto world = make_test_world();
    save_system_->save(world, 1);
    save_system_->save(world, 3);
    save_system_->save(world, 5);

    auto slots = save_system_->list_slots();
    ASSERT_EQ(slots.size(), 3);
    EXPECT_EQ(slots[0].slot, 1);
    EXPECT_EQ(slots[1].slot, 3);
    EXPECT_EQ(slots[2].slot, 5);
}

TEST_F(SaveSystemTest, ListSlotsEmptyDir) {
    auto slots = save_system_->list_slots();
    EXPECT_TRUE(slots.empty());
}

TEST_F(SaveSystemTest, SlotExistsTrue) {
    auto world = make_test_world();
    save_system_->save(world, 1);
    EXPECT_TRUE(save_system_->slot_exists(1));
}

TEST_F(SaveSystemTest, SlotExistsFalse) { EXPECT_FALSE(save_system_->slot_exists(2)); }

TEST_F(SaveSystemTest, DeleteSlotRemovesFile) {
    auto world = make_test_world();
    save_system_->save(world, 1);
    ASSERT_TRUE(save_system_->slot_exists(1));

    save_system_->delete_slot(1);
    EXPECT_FALSE(save_system_->slot_exists(1));
}

TEST_F(SaveSystemTest, SaveSlotInfoContainsMetadata) {
    auto world = make_test_world();
    save_system_->save(world, 1);

    auto slots = save_system_->list_slots();
    ASSERT_EQ(slots.size(), 1);
    EXPECT_EQ(slots[0].slot, 1);
    EXPECT_EQ(slots[0].location_name, "The Broken Wheel Tavern");
    EXPECT_EQ(slots[0].clock_display, "Evening, Day 2");
    EXPECT_FALSE(slots[0].timestamp.empty());
}

TEST_F(SaveSystemTest, SaveCreatesDirectory) {
    auto nested_dir = test_dir_ / "deeply" / "nested" / "saves";
    SaveSystem nested_save(nested_dir);

    auto world = make_test_world();
    nested_save.save(world, 1);

    EXPECT_TRUE(std::filesystem::exists(nested_dir / "slot_1.json"));
}

TEST_F(SaveSystemTest, SchemaVersionInSaveData) {
    auto world = make_test_world();
    save_system_->save(world, 1);

    auto result = save_system_->load(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->schema_version, SaveData::kCurrentSchemaVersion);
}
