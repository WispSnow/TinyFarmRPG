// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::loader {
namespace {

TEST(EntityBuilderMerchantSourceTest, BuildActorReadsShopIdAndKeepsMerchantPriority) {
    const std::filesystem::path conventions_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/loader/tiled_conventions.h").lexically_normal();
    const std::filesystem::path builder_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/loader/entity_builder.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(conventions_path)) << conventions_path;
    ASSERT_TRUE(std::filesystem::exists(builder_path)) << builder_path;

    const std::string conventions = test_source_utils::readTextFile(conventions_path);
    const std::string builder = test_source_utils::readTextFile(builder_path);
    ASSERT_FALSE(conventions.empty());
    ASSERT_FALSE(builder.empty());

    EXPECT_NE(conventions.find("ACTOR_PROP_SHOP_ID"), std::string::npos);
    EXPECT_NE(conventions.find("\"shop_id\""), std::string::npos);
    EXPECT_NE(conventions.find("ACTOR_PROP_ACTOR_ID"), std::string::npos);
    EXPECT_NE(conventions.find("\"actor_id\""), std::string::npos);
    EXPECT_NE(conventions.find("ACTOR_PROP_BATTLE_TROOP_ID"), std::string::npos);
    EXPECT_NE(conventions.find("\"battle_troop_id\""), std::string::npos);

    const std::string build_actor_block = test_source_utils::extractFunctionBlock(builder, "void EntityBuilder::buildActor(entt::id_type name_id)");
    ASSERT_FALSE(build_actor_block.empty());
    EXPECT_NE(build_actor_block.find("ACTOR_PROP_ACTOR_ID"), std::string::npos);
    EXPECT_NE(build_actor_block.find("setActorIdentity(registry_, entity_id_, *actor_id)"), std::string::npos);
    EXPECT_NE(build_actor_block.find("MerchantComponent"), std::string::npos);
    EXPECT_NE(build_actor_block.find("ACTOR_PROP_SHOP_ID"), std::string::npos);
    EXPECT_NE(build_actor_block.find("else if (quest_offer_id)"), std::string::npos);
    EXPECT_NE(build_actor_block.find("merchant 优先处理"), std::string::npos);
    EXPECT_NE(build_actor_block.find("ACTOR_PROP_BATTLE_TROOP_ID"), std::string::npos);
    EXPECT_NE(build_actor_block.find("EnemyEncounterComponent"), std::string::npos);
    EXPECT_NE(build_actor_block.find("ACTOR_PROP_RESPAWN_ON_MAP_RELOAD"), std::string::npos);
    EXPECT_NE(build_actor_block.find("!respawn_on_map_reload && isEncounterDefeated(*encounter_id)"), std::string::npos);
    EXPECT_NE(build_actor_block.find("本阶段忽略战斗入口"), std::string::npos);

    const auto shop_pos = build_actor_block.find("ACTOR_PROP_SHOP_ID");
    const auto quest_pos = build_actor_block.find("ACTOR_PROP_QUEST_OFFER_ID");
    ASSERT_NE(shop_pos, std::string::npos);
    ASSERT_NE(quest_pos, std::string::npos);
    EXPECT_LT(shop_pos, quest_pos);
}

} // namespace
} // namespace game::loader
// NOLINTEND
