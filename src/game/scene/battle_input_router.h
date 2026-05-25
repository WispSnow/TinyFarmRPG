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
    void update(float delta_time);
    void clearRepeat();

    [[nodiscard]] bool connected() const { return connected_; }

private:
    enum class RepeatDirection {
        None,
        Up,
        Down,
        Left,
        Right
    };

    [[nodiscard]] bool onMenuUpPressed();
    [[nodiscard]] bool onMenuDownPressed();
    [[nodiscard]] bool onMenuLeftPressed();
    [[nodiscard]] bool onMenuRightPressed();
    [[nodiscard]] bool onMenuConfirmPressed();
    [[nodiscard]] bool onMenuCancelPressed();

    [[nodiscard]] bool moveVertical(int direction);
    [[nodiscard]] bool moveHorizontal(int direction);
    [[nodiscard]] bool repeatDirectionStillDown() const;
    [[nodiscard]] bool dispatchRepeatDirection();
    void beginRepeat(RepeatDirection direction);

    engine::input::InputManager* input_manager_{nullptr};
    Delegate* delegate_{nullptr};
    bool connected_{false};
    RepeatDirection repeat_direction_{RepeatDirection::None};
    float repeat_timer_seconds_{0.0f};
};

} // namespace game::scene
