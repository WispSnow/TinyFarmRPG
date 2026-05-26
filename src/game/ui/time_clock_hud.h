#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <string>

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::data {
struct GameTime;
}

namespace game::runtime {
class LocalizationService;
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
                 uint64_t owner_scene_id,
                 const game::runtime::LocalizationService* localization);
    ~TimeClockHud();

    TimeClockHud(const TimeClockHud&) = delete;
    TimeClockHud& operator=(const TimeClockHud&) = delete;

    /// 每帧从 GameTime 刷新 data model
    void update(const game::data::GameTime* game_time);
    void onLanguageChanged(const game::data::GameTime* game_time);

private:
    engine::ui::rmlui::RmlDocumentController document_controller_{};
    const game::runtime::LocalizationService* localization_{nullptr};

    // 绑定数据
    Rml::String day_text_{"!hud.day!"};
    Rml::String time_text_{"??:??"};
    Rml::String hand_decorator_{"image(clock-hand-0)"};

    // 脏检测缓存
    int last_day_{-1};
    int last_hour_{-1};
    int last_minute_{-1};

    [[nodiscard]] static int pickHandIndex(float hour, float minute);
    [[nodiscard]] static std::string formatHandDecorator(int index);
    [[nodiscard]] std::string formatDayText(std::string_view day) const;
};

} // namespace game::ui
