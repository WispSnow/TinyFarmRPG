#pragma once

#include "engine/scene/scene.h"
#include "game/defs/events.h"

#include <memory>
#include <optional>

namespace game::data {
    struct GameTime;
}

namespace game::ui {
    class InventoryUI;
    class HotbarUI;
    class DialogueBubble;
    class ItemTooltipUI;
}

namespace engine::ui {
    class UIScreenFade;
}

namespace game::runtime {
    struct GameRuntimeServices;
    struct GameSystemBundle;
}

namespace game::scene {

class GameScene : public engine::scene::Scene {
    std::unique_ptr<game::runtime::GameRuntimeServices> services_;
    std::unique_ptr<game::runtime::GameSystemBundle> systems_;

    std::shared_ptr<game::data::GameTime> game_time_;
    std::optional<int> load_slot_{};
    bool abort_to_title_{false};

    game::ui::InventoryUI* inventory_ui_{nullptr};
    game::ui::HotbarUI* hotbar_ui_{nullptr};
    game::ui::DialogueBubble* dialogue_bubble_{nullptr};
    game::ui::ItemTooltipUI* item_tooltip_ui_{nullptr};
    engine::ui::UIScreenFade* screen_fade_{nullptr};

public:
    GameScene(std::string_view name, engine::core::Context& context,
              std::shared_ptr<game::data::GameTime> game_time = nullptr,
              std::optional<int> load_slot = std::nullopt);
    ~GameScene() noexcept override;

    bool init() override;
    void update(float delta_time) override;
    void render() override;

    void clean() override;

private:
    void bindSceneInputActions();
    [[nodiscard]] bool initUI();  // 在具体场景中初始化UI管理器，且位置要靠后，确保按键注册顺序正确
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool registerDebugPanels();
#endif

    bool onInventoryToggle();
    bool onHotbarToggle();
    bool onPauseToggle();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);
    void onHotbarSlotChanged(const game::defs::HotbarSlotChanged& evt);
};

} // namespace game::scene
