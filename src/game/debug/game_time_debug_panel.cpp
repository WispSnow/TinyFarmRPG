#include "game_time_debug_panel.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string_view>

namespace {

[[nodiscard]] inline std::string_view timeOfDayToString(game::data::TimeOfDay time_of_day) {
    switch (time_of_day) {
        case game::data::TimeOfDay::Dawn: return "黎明";
        case game::data::TimeOfDay::Day: return "白天";
        case game::data::TimeOfDay::Dusk: return "黄昏";
        case game::data::TimeOfDay::Night: return "夜晚";
        default: return "未知";
    }
}

} // namespace

namespace game::debug {

GameTimeDebugPanel::GameTimeDebugPanel(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
}

std::string_view GameTimeDebugPanel::name() const {
    return "Game Time";
}

void GameTimeDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Game Time Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // 获取游戏时间数据
    auto* game_time_it = registry_.ctx().find<game::data::GameTime>();
    if (!game_time_it) {
        ImGui::Text("未找到游戏时间数据");
        ImGui::End();
        return;
    }

    auto& game_time = *game_time_it;

    // 显示当前时间
    ImGui::Text("当前时间: %s", game_time.getFormattedTime().c_str());
    ImGui::Separator();

    // 显示当前时段
    const auto tod_str = timeOfDayToString(game_time.time_of_day_);
    // 使用 %.*s 格式化说明符安全地打印 string_view：
    // - %.*s 中的 .* 表示精度（长度）由参数动态指定
    // - 第一个参数是长度（size_t 转为 int），第二个参数是字符串指针
    // - 这种方式不依赖 null 终止符，适合处理 string_view 等非 null 终止的字符串
    ImGui::Text("当前时段: %.*s", static_cast<int>(tod_str.size()), tod_str.data());
    ImGui::Separator();

    // 时间控制
    ImGui::Text("时间控制");

    if (ImGui::Button("快进 1 小时")) {
        dispatcher_.enqueue(game::defs::AdvanceTimeRequest{1});
    }
    ImGui::SameLine();
    if (ImGui::Button("快进 1 天")) {
        dispatcher_.enqueue(game::defs::AdvanceTimeRequest{24});
    }
    ImGui::Separator();

    // 暂停/恢复按钮
    if (game_time.paused_) {
        if (ImGui::Button("恢复时间")) {
            game_time.paused_ = false;
            spdlog::info("游戏时间已恢复");
        }
    } else {
        if (ImGui::Button("暂停时间")) {
            game_time.paused_ = true;
            spdlog::info("游戏时间已暂停");
        }
    }
    ImGui::SameLine();
    ImGui::Text("状态: %s", game_time.paused_ ? "已暂停" : "运行中");
    ImGui::Separator();

    // 时间缩放因子滑块
    float time_scale = game_time.time_scale_;
    if (ImGui::SliderFloat("时间缩放", &time_scale, 0.01f, 100.0f, "%.2fx")) {
        game_time.time_scale_ = time_scale;
    }
    ImGui::Text("当前缩放: %.2fx", game_time.time_scale_);
    ImGui::Separator();

    // 手动设置时间（可选功能）
    if (ImGui::CollapsingHeader("手动设置时间")) {
        ImGui::TextUnformatted("注意：手动设置只修改数值，不会触发 DayChangedEvent/HourChangedEvent。");
        ImGui::TextUnformatted("若要推动作物生长/日结算，请使用上方“快进”按钮。");
        ImGui::Separator();

        int day = static_cast<int>(game_time.day_);
        float hour = game_time.hour_;
        float minute = game_time.minute_;

        bool time_changed = false;

        if (ImGui::InputInt("天数", &day)) {
            if (day < 1) day = 1;
            time_changed = true;
        }

        if (ImGui::SliderFloat("小时", &hour, 0.0f, 23.99f, "%.2f")) {
            time_changed = true;
        }

        if (ImGui::SliderFloat("分钟", &minute, 0.0f, 59.99f, "%.2f")) {
            time_changed = true;
        }

        if (time_changed) {
            game_time.day_ = static_cast<std::uint32_t>(day);
            game_time.hour_ = hour;
            game_time.minute_ = minute;
            // 重新计算时段
            game_time.time_of_day_ = game_time.calculateTimeOfDay(hour);
            spdlog::info("手动设置时间：Day {}, {:.2f}:{:.2f}", day, hour, minute);
        }

        if (ImGui::Button("重置为默认时间")) {
            game_time.day_ = 1;
            game_time.hour_ = 6.0f;
            game_time.minute_ = 0.0f;
            game_time.time_of_day_ = game_time.calculateTimeOfDay(6.0f);
            spdlog::info("时间已重置为默认值");
        }
    }

    // 显示配置信息
    if (ImGui::CollapsingHeader("时间配置")) {
        ImGui::Text("每分钟流逝速度: %.2f 分钟/秒", game_time.config_.minutes_per_real_second_);
        ImGui::Separator();
        ImGui::Text("时段时间点:");
        ImGui::BulletText("黎明: %.2f:00 - %.2f:00", 
                         game_time.config_.dawn_start_hour_, 
                         game_time.config_.dawn_end_hour_);
        ImGui::BulletText("白天: %.2f:00 - %.2f:00", 
                         game_time.config_.day_start_hour_, 
                         game_time.config_.day_end_hour_);
        ImGui::BulletText("黄昏: %.2f:00 - %.2f:00", 
                         game_time.config_.dusk_start_hour_, 
                         game_time.config_.dusk_end_hour_);
        ImGui::BulletText("夜晚: %.2f:00 - %.2f:00", 
                         game_time.config_.night_start_hour_, 
                         game_time.config_.night_end_hour_);
    }

    ImGui::End();
}

} // namespace game::debug
