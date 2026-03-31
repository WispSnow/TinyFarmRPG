#pragma once

#include "engine/ui/rmlui/rml_data_bridge.h"

#include <RmlUi/Core/Types.h>

#include <string>

namespace Rml {
class ElementDocument;
class Context;
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::data {
struct GameTime;
}

namespace game::ui {

/**
 * @brief RmlUi 驱动的时钟 HUD
 *
 * 通过 data binding 将 GameTime 映射到 time_clock.rml 中的
 * day_text / time_text / hand_decorator 三个变量。
 */
class TimeClockHud final {
public:
    /// @param owner_scene_id 场景实例 ID，用于 RmlUiRuntime 文档归属管理
    TimeClockHud(engine::ui::rmlui::RmlUiRuntime& runtime,
                 uint64_t owner_scene_id);
    ~TimeClockHud();

    TimeClockHud(const TimeClockHud&) = delete;
    TimeClockHud& operator=(const TimeClockHud&) = delete;

    /// 每帧从 GameTime 刷新 data model
    void update(const game::data::GameTime* game_time);

private:
    engine::ui::rmlui::RmlUiRuntime& runtime_;
    engine::ui::rmlui::RmlDataBridge data_bridge_;
    Rml::ElementDocument* document_{nullptr};

    // 绑定数据
    Rml::String day_text_{"Day --"};
    Rml::String time_text_{"??:??"};
    Rml::String hand_decorator_{"image(clock-hand-0)"};

    // 脏检测缓存
    int last_day_{-1};
    int last_hour_{-1};
    int last_minute_{-1};

    [[nodiscard]] static int pickHandIndex(float hour, float minute);
    [[nodiscard]] static std::string formatHandDecorator(int index);
};

} // namespace game::ui
