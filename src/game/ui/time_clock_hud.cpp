#include "time_clock_hud.h"

#include "game/data/game_time.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
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

TimeClockHud::TimeClockHud(engine::ui::rmlui::RmlUILayer& layer,
                           Rml::Context* context,
                           uint64_t owner_scene_id)
    : layer_(layer) {
    if (!context) {
        spdlog::error("TimeClockHud: context is null.");
        return;
    }

    auto constructor = data_bridge_.create(context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("TimeClockHud: failed to create data model '{}'.", MODEL_NAME);
        return;
    }

    constructor.Bind("day_text", &day_text_);
    constructor.Bind("time_text", &time_text_);
    constructor.Bind("hand_decorator", &hand_decorator_);

    document_ = layer_.loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("TimeClockHud: failed to load '{}'.", DOCUMENT_PATH);
        return;
    }

    layer_.showDocument(document_);
    spdlog::debug("TimeClockHud 初始化完成。");
}

TimeClockHud::~TimeClockHud() {
    if (document_) {
        layer_.unloadDocument(document_);
        document_ = nullptr;
    }
    data_bridge_.destroy();
}

void TimeClockHud::update(const game::data::GameTime* game_time) {
    if (!data_bridge_.isValid()) {
        return;
    }

    if (!game_time) {
        // Fallback 显示
        if (last_day_ != -1 || last_hour_ != -1 || last_minute_ != -1) {
            day_text_ = "Day --";
            time_text_ = "??:??";
            hand_decorator_ = formatHandDecorator(0);
            last_day_ = -1;
            last_hour_ = -1;
            last_minute_ = -1;
            data_bridge_.markAllDirty();
        }
        return;
    }

    const int day = static_cast<int>(game_time->day_);
    const int hour = static_cast<int>(game_time->hour_);
    const int minute = static_cast<int>(game_time->minute_);
    bool dirty = false;

    if (day != last_day_) {
        day_text_ = std::format("Day {}", game_time->day_);
        data_bridge_.markDirty("day_text");
        last_day_ = day;
        dirty = true;
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
        data_bridge_.markDirty("time_text");

        // 更新指针帧
        const int hand_index = pickHandIndex(game_time->hour_, game_time->minute_);
        hand_decorator_ = formatHandDecorator(hand_index);
        data_bridge_.markDirty("hand_decorator");

        last_hour_ = hour;
        last_minute_ = minute;
        dirty = true;
    }

    (void)dirty;
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

} // namespace game::ui
