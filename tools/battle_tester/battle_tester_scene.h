#pragma once

#include "engine/scene/scene.h"
#include "game/battle/battle_types.h"
#include "game/data/appearance_catalog.h"
#include "game/data/battle_background_id.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/factory/blueprint_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace game::defs {
struct BattleEndedEvent;
}

namespace tools::battle_tester {

struct BattleTesterConfig {
    std::vector<std::string> actor_ids{"actor.player", "actor.lyria", "actor.tori"};
    std::string troop_id{"troop.goblin_pair"};
    std::string battle_background_id{game::data::DEFAULT_BATTLE_BACKGROUND_ID};
    int potion_count{5};
};

/// @brief 战斗测试工具的栈底场景。
///
/// 该场景持有 BattleScene 使用的 catalog/manager，并在 BattleScene 存活期间保持不销毁、不重建。
class BattleTesterScene final : public engine::scene::Scene {
public:
    BattleTesterScene(std::string_view name, engine::core::Context& context, BattleTesterConfig config);
    ~BattleTesterScene() override;

    [[nodiscard]] bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool loadResources();
    [[nodiscard]] bool launchBattle();
    [[nodiscard]] std::unique_ptr<engine::scene::Scene> createBattleScene();
    void handleRootShortcuts();
    void onBattleEnded(const game::defs::BattleEndedEvent& evt);

    BattleTesterConfig config_{};
    game::data::ItemCatalog item_catalog_{};
    game::data::RpgCatalog rpg_catalog_{};
    game::factory::BlueprintManager blueprint_manager_{};
    game::data::AppearanceCatalog appearance_catalog_{};
    std::optional<game::battle::BattleOutcome> last_outcome_{};
    bool battle_active_{false};
    bool launch_requested_{false};
    bool previous_restart_pressed_{false};
    bool previous_escape_pressed_{false};
};

} // namespace tools::battle_tester
