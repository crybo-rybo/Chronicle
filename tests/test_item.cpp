#include "entities/item.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(ItemTest, DefaultConstruction) {
    Item item;
    EXPECT_TRUE(item.id.empty());
    EXPECT_TRUE(item.name.empty());
    EXPECT_TRUE(item.description.empty());
    EXPECT_TRUE(item.takeable);
    EXPECT_FALSE(item.key_item);
    EXPECT_FALSE(item.hidden);
    EXPECT_TRUE(item.unlock_target.empty());
    EXPECT_TRUE(item.properties.empty());
}

// ---------------------------------------------------------------------------
// JSON roundtrip — all fields
// ---------------------------------------------------------------------------

TEST(ItemTest, JsonRoundtrip) {
    Item original;
    original.id            = "iron_key";
    original.name          = "Iron Key";
    original.description   = "A heavy iron key.";
    original.takeable      = false;
    original.key_item      = true;
    original.hidden        = true;
    original.unlock_target = "north_door";
    original.properties    = {{"material", "iron"}, {"weight", "heavy"}};

    nlohmann::json j;
    to_json(j, original);

    Item restored;
    from_json(j, restored);

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.description, original.description);
    EXPECT_EQ(restored.takeable, original.takeable);
    EXPECT_EQ(restored.key_item, original.key_item);
    EXPECT_EQ(restored.hidden, original.hidden);
    EXPECT_EQ(restored.unlock_target, original.unlock_target);
    EXPECT_EQ(restored.properties, original.properties);
}

// ---------------------------------------------------------------------------
// Properties map survives roundtrip
// ---------------------------------------------------------------------------

TEST(ItemTest, PropertiesMapRoundtrip) {
    Item item;
    item.id   = "crumpled_note";
    item.name = "Crumpled Note";
    item.properties = {
        {"readable", "true"},
        {"read_text", "Don't trust the innkeeper."},
    };

    nlohmann::json j;
    to_json(j, item);

    Item restored;
    from_json(j, restored);

    ASSERT_EQ(restored.properties.size(), 2u);
    EXPECT_EQ(restored.properties.at("readable"), "true");
    EXPECT_EQ(restored.properties.at("read_text"), "Don't trust the innkeeper.");
}

// ---------------------------------------------------------------------------
// id IS written to JSON in to_json (set from map key by loader but also serialized)
// ---------------------------------------------------------------------------

TEST(ItemTest, IdWrittenToJson) {
    Item item;
    item.id   = "some_item";
    item.name = "Some Item";

    nlohmann::json j;
    to_json(j, item);

    EXPECT_TRUE(j.contains("id"));
    EXPECT_EQ(j["id"].get<std::string>(), "some_item");
}

// ---------------------------------------------------------------------------
// from_json uses defaults when fields are absent
// ---------------------------------------------------------------------------

TEST(ItemTest, FromJsonDefaults) {
    nlohmann::json j = nlohmann::json::object();

    Item item;
    from_json(j, item);

    EXPECT_TRUE(item.id.empty());
    EXPECT_TRUE(item.name.empty());
    EXPECT_TRUE(item.description.empty());
    EXPECT_TRUE(item.takeable);
    EXPECT_FALSE(item.key_item);
    EXPECT_FALSE(item.hidden);
    EXPECT_TRUE(item.unlock_target.empty());
    EXPECT_TRUE(item.properties.empty());
}

} // namespace chronicle
