#pragma once

#include "engine/debug/debug_panel.h"
#include "engine/audio/audio_player.h"

namespace engine::debug {

class AudioDebugPanel final : public DebugPanel {
    engine::audio::AudioPlayer& audio_player_;
    float music_volume_{1.0f};
    float sound_volume_{1.0f};
    float spatial_falloff_distance_{0.0f};
    float spatial_pan_range_{0.0f};

public:
    explicit AudioDebugPanel(engine::audio::AudioPlayer& audio_player);

    std::string_view name() const override;
    void draw(bool& is_open) override;
    void onShow() override;

private:
    void syncFromAudio();
};

} // namespace engine::debug
