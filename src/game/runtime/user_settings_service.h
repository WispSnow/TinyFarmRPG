#pragma once

#include "game/runtime/user_settings.h"

#include <entt/signal/fwd.hpp>

#include <string_view>

namespace engine::audio {
class AudioPlayer;
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::data {
struct GameTime;
}

namespace game::runtime {

/// @brief 玩家偏好设置的唯一真源。
///
/// 持久化策略：
///   - 启动时优先读 build 目录下 `config/user_settings.json`；
///     若不存在则退回出厂模板 `config/user_settings.default.json`；
///     若两者都缺，使用代码内默认值。
///   - setter 立即 apply 到对应子系统并标记 dirty。
///   - `flushIfDirty()` 把当前值写回到 `config/user_settings.json`（仅 build 目录）。
///
/// 跨层约束：service 在 game 层，向 engine 反向调 setter 合法；engine 不持有 service。
class UserSettingsService final {
public:
    /// @brief 持久化目标路径（runtime 写入位置）。
    static constexpr std::string_view USER_SETTINGS_PATH{"config/user_settings.json"};
    /// @brief 出厂模板路径。
    static constexpr std::string_view DEFAULT_SETTINGS_PATH{"config/user_settings.default.json"};

    UserSettingsService(entt::dispatcher& dispatcher,
                        engine::audio::AudioPlayer& audio_player,
                        game::data::GameTime& game_time,
                        engine::ui::rmlui::RmlUiRuntime& rml_runtime) noexcept;

    UserSettingsService(const UserSettingsService&) = delete;
    UserSettingsService& operator=(const UserSettingsService&) = delete;
    UserSettingsService(UserSettingsService&&) = delete;
    UserSettingsService& operator=(UserSettingsService&&) = delete;

    /// @brief 优先读用户文件，缺失则退回 default 文件；都缺则保持代码默认值。
    /// @return true 表示有任意一个文件被成功 load；false 表示走代码默认值。
    bool loadFromFileOrFallback(std::string_view user_path = USER_SETTINGS_PATH,
                                std::string_view default_path = DEFAULT_SETTINGS_PATH);

    /// @brief 将当前 settings 反序列化并立即 apply 到 audio / time / (UI 在 Phase 4 接入)。
    void applyAll();

    /// @brief 写回到指定路径。仅在 `dirty_` 为 true 时实际写盘。
    /// @return true 表示成功（含无需写）；false 表示写盘失败。
    bool flushIfDirty(std::string_view path = USER_SETTINGS_PATH);

    /// @brief 强制写盘一次（忽略 dirty flag）。主要用于测试 / 工具。
    bool saveToFile(std::string_view path) const;

    /// @brief 从指定文件载入（忽略默认 fallback）。public 仅用于测试。
    bool loadFromFile(std::string_view path);

    [[nodiscard]] const UserSettings& snapshot() const noexcept { return settings_; }
    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }

    // ----- Setters：写值 + apply + 派发事件 + mark dirty -----
    void setMusicVolume(float value);
    void setSoundVolume(float value);
    void setGlobalTimeScale(float value);
    void setBattleAnimationSpeed(float value);
    void setShowDamagePopup(bool visible);
    void setShowEnemyHpBar(bool visible);
    void setCursorMemory(bool enabled);
    void setUiFontScale(UiFontScale scale);

private:
    void applyAudio();
    void applyTimeScale();
    void applyUiFontScale();

    entt::dispatcher& dispatcher_;
    engine::audio::AudioPlayer& audio_player_;
    game::data::GameTime& game_time_;
    engine::ui::rmlui::RmlUiRuntime& rml_runtime_;

    UserSettings settings_{};
    bool dirty_{false};
};

} // namespace game::runtime
