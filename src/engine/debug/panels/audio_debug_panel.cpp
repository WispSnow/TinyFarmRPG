#include "audio_debug_panel.h"
#include <imgui.h>

namespace engine::debug {

namespace {
constexpr entt::hashed_string TEST_UI_CLICK{"ui_click"};
constexpr entt::hashed_string TEST_UI_HOVER{"ui_hover"};
constexpr entt::hashed_string TEST_TITLE_BG_MUSIC{"title-bg-music"};
constexpr entt::hashed_string TEST_SCENE_BG_MUSIC{"scene-bg-music"};
constexpr int TEST_MUSIC_FADE_IN_MS = 200;
} // namespace

AudioDebugPanel::AudioDebugPanel(engine::audio::AudioPlayer& audio_player)
    : audio_player_(audio_player) {
}

std::string_view AudioDebugPanel::name() const {
    return "Audio";
}

void AudioDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Audio Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Music Volume: %.2f", music_volume_);
    ImGui::Text("Sound Volume: %.2f", sound_volume_);
    ImGui::Text("Spatial Falloff: %.1f", spatial_falloff_distance_);
    ImGui::Text("Spatial Pan Range: %.1f", spatial_pan_range_);
    ImGui::Separator();

    float music = music_volume_;
    // 这里的“##”后面的内容不会显示，但是会参与到哈希值计算中，可避免哈希冲突
    if (ImGui::SliderFloat("Music##volume", &music, 0.0f, 1.0f, "%.2f")) {
        music_volume_ = music;
        audio_player_.setMusicVolume(music_volume_);
    }

    float sound = sound_volume_;
    if (ImGui::SliderFloat("Sound##volume", &sound, 0.0f, 1.0f, "%.2f")) {
        sound_volume_ = sound;
        audio_player_.setSoundVolume(sound_volume_);
    }

    float falloff = spatial_falloff_distance_;
    if (ImGui::DragFloat("Falloff Distance##spatial", &falloff, 5.0f, 0.0f, 5000.0f, "%.1f")) {
        spatial_falloff_distance_ = falloff;
        audio_player_.setSpatialFalloffDistance(spatial_falloff_distance_);
    }

    float pan_range = spatial_pan_range_;
    if (ImGui::DragFloat("Pan Range##spatial", &pan_range, 2.0f, 0.0f, 2000.0f, "%.1f")) {
        spatial_pan_range_ = pan_range;
        audio_player_.setSpatialPanRange(spatial_pan_range_);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Test:");
    if (ImGui::Button("Play ui_click##audio_test")) {
        [[maybe_unused]] const bool played = audio_player_.playSound(TEST_UI_CLICK.value());
    }
    ImGui::SameLine();
    if (ImGui::Button("Play ui_hover##audio_test")) {
        [[maybe_unused]] const bool played = audio_player_.playSound(TEST_UI_HOVER.value());
    }

    if (ImGui::Button("Play title music##audio_test")) {
        [[maybe_unused]] const bool started = audio_player_.playMusic(TEST_TITLE_BG_MUSIC.value(), true, TEST_MUSIC_FADE_IN_MS);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play scene music##audio_test")) {
        [[maybe_unused]] const bool started = audio_player_.playMusic(TEST_SCENE_BG_MUSIC.value(), true, TEST_MUSIC_FADE_IN_MS);
    }

    if (ImGui::Button("Pause Music")) {
        audio_player_.pauseMusic();
    }
    ImGui::SameLine();
    if (ImGui::Button("Resume Music")) {
        audio_player_.resumeMusic();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Music")) {
        audio_player_.stopMusic();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Config:");
    if (ImGui::Button("Reload##audio_config")) {
        [[maybe_unused]] const bool loaded = audio_player_.loadConfig(engine::audio::AudioPlayer::DEFAULT_CONFIG_PATH);
        syncFromAudio();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save##audio_config")) {
        [[maybe_unused]] const bool saved = audio_player_.saveConfig(engine::audio::AudioPlayer::DEFAULT_CONFIG_PATH);
    }

    ImGui::End();
}

void AudioDebugPanel::onShow() {
    syncFromAudio();
}

void AudioDebugPanel::syncFromAudio() {
    music_volume_ = audio_player_.getMusicVolume();
    sound_volume_ = audio_player_.getSoundVolume();
    spatial_falloff_distance_ = audio_player_.getSpatialFalloffDistance();
    spatial_pan_range_ = audio_player_.getSpatialPanRange();
}

} // namespace engine::debug
