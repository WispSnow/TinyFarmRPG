#include "time_clock_hud.h"

#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/data/game_time.h"
#include "game/ui/localized_text.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace {
// 指针区间：以 10:30 为起点，每 180 分钟一档
constexpr float SEGMENT_START_MIN = 630.0f; // 10:30
constexpr float MINUTES_PER_SEGMENT = 180.0f;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/time_clock.rml";
constexpr std::string_view MODEL_NAME = "time_clock";
} // namespace

namespace game::ui {

TimeClockHud::TimeClockHud(engine::ui::rmlui::RmlUiRuntime& runtime,
                           uint64_t owner_scene_id,
                           const game::runtime::LocalizationService* localization)
    : localization_(localization) {
    document_controller_.attach(&runtime, owner_scene_id);
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("TimeClockHud: failed to create data model '{}'.", MODEL_NAME);
        return;
    }

    constructor.Bind("day_text", &day_text_);
    constructor.Bind("time_text", &time_text_);
    constructor.Bind("hand_decorator", &hand_decorator_);
    day_text_ = formatDayText("--");

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("TimeClockHud: failed to load '{}'.", DOCUMENT_PATH);
        document_controller_.unload();
        return;
    }

    document_controller_.markAllDirty();
    spdlog::debug("TimeClockHud 初始化完成。");
}

TimeClockHud::~TimeClockHud() {
    document_controller_.unload();
}

void TimeClockHud::update(const game::data::GameTime* game_time) {
    if (!document_controller_.isModelValid()) {
        return;
    }

    if (!game_time) {
        // Fallback 显示
        if (last_day_ != -1 || last_hour_ != -1 || last_minute_ != -1) {
            day_text_ = formatDayText("--");
            time_text_ = "??:??";
            hand_decorator_ = formatHandDecorator(0);
            last_day_ = -1;
            last_hour_ = -1;
            last_minute_ = -1;
            document_controller_.markAllDirty();
        }
        return;
    }

    const int day = static_cast<int>(game_time->day_);
    const int hour = static_cast<int>(game_time->hour_);
    const int minute = static_cast<int>(game_time->minute_);

    if (day != last_day_) {
        day_text_ = formatDayText(std::to_string(game_time->day_));
        document_controller_.markDirty("day_text");
        last_day_ = day;
    }

    if (hour != last_hour_ || minute != last_minute_) {
        // 更新时间文本
        int total_minutes = static_cast<int>(std::lround(game_time->hour_ * 60.0f + game_time->minute_));
        if (total_minutes < 0) {
            total_minutes = 0;
        }
        const int display_hour = (total_minutes / 60) % 24;
        const int display_minute = total_minutes % 60;
        time_text_ = std::format("{:02d}:{:02d}", display_hour, display_minute);
        document_controller_.markDirty("time_text");

        // 更新指针帧
        const int hand_index = pickHandIndex(game_time->hour_, game_time->minute_);
        hand_decorator_ = formatHandDecorator(hand_index);
        document_controller_.markDirty("hand_decorator");

        last_hour_ = hour;
        last_minute_ = minute;
    }
}

void TimeClockHud::onLanguageChanged(const game::data::GameTime* game_time) {
    last_day_ = -2;
    update(game_time);
}

int TimeClockHud::pickHandIndex(float hour, float minute) {
    float total_minutes = hour * 60.0f + minute;
    total_minutes = std::fmod(total_minutes, 1440.0f);
    if (total_minutes < 0.0f) {
        total_minutes += 1440.0f;
    }
    const float normalized = std::fmod(total_minutes - SEGMENT_START_MIN + 1440.0f, 1440.0f);
    const int index = static_cast<int>(std::floor(normalized / MINUTES_PER_SEGMENT));
    return std::clamp(index, 0, 7);
}

std::string TimeClockHud::formatHandDecorator(int index) {
    return std::format("image(clock-hand-{})", std::clamp(index, 0, 7));
}

std::string TimeClockHud::formatDayText(std::string_view day) const {
    return game::ui::formatTextOrFallback(
        localization_,
        "hud.day",
        {{"day", std::string(day)}},
        [day] { return "Day " + std::string(day); });
}

} // namespace game::ui
