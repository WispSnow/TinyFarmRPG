#pragma once

#include "game/scene/battle_scene_state.h"

namespace engine::input {
class InputManager;
}

namespace game::scene {

/// @brief 连接战斗菜单输入动作，并把方向输入转换为菜单游标移动。
class BattleInputRouter final {
public:
    class Delegate {
    public:
        virtual ~Delegate() = default;

        [[nodiscard]] virtual BattleMenuState battleMenuState() const = 0;
        virtual bool moveBattleMenuCursor(int delta) = 0;
        virtual bool confirmBattleMenu() = 0;
        virtual bool cancelBattleMenu() = 0;
    };

    ~BattleInputRouter();

    void connect(engine::input::InputManager& input_manager, Delegate& delegate);
    void disconnect();

    [[nodiscard]] bool connected() const { return connected_; }

private:
    [[nodiscard]] bool onMenuUpPressed();
    [[nodiscard]] bool onMenuDownPressed();
    [[nodiscard]] bool onMenuLeftPressed();
    [[nodiscard]] bool onMenuRightPressed();
    [[nodiscard]] bool onMenuConfirmPressed();
    [[nodiscard]] bool onMenuCancelPressed();

    [[nodiscard]] bool moveVertical(int direction);
    [[nodiscard]] bool moveHorizontal(int direction);

    engine::input::InputManager* input_manager_{nullptr};
    Delegate* delegate_{nullptr};
    bool connected_{false};
};

} // namespace game::scene
