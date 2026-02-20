#include <gtest/gtest.h>

#include <concepts>
#include <string_view>
#include <vector>

#include "engine/resource/audio_manager.h"
#include "engine/resource/font_manager.h"
#include "engine/resource/texture_manager.h"

namespace engine::resource {
namespace {

template <typename T>
concept TextureManagerApiVisible = requires(T& manager,
                                            entt::id_type id,
                                            std::string_view path,
                                            std::vector<TextureDebugInfo>& infos) {
    { manager.loadTexture(id, path) } -> std::same_as<TextureHandle>;
    { manager.findTexture(id) } -> std::same_as<TextureHandle>;
    manager.unloadTexture(id);
    manager.clearTextures();
    manager.collectDebugInfo(infos);
};

template <typename T>
concept AudioManagerApiVisible = requires(T& manager,
                                          entt::id_type id,
                                          std::string_view path,
                                          std::vector<AudioDebugInfo>& infos) {
    { manager.findSound(id) } -> std::same_as<AudioBufferHandle>;
    { manager.findMusic(id) } -> std::same_as<AudioBufferHandle>;
    { manager.loadSound(id, path) } -> std::same_as<AudioBufferHandle>;
    { manager.loadMusic(id, path) } -> std::same_as<AudioBufferHandle>;
    manager.unloadSound(id);
    manager.unloadMusic(id);
    manager.clearSounds();
    manager.clearMusic();
    manager.clearAudio();
    manager.collectSoundDebugInfo(infos);
    manager.collectMusicDebugInfo(infos);
};

template <typename T>
concept FontManagerApiVisible = requires(T& manager,
                                         entt::id_type id,
                                         int pixel_size,
                                         std::string_view path,
                                         std::vector<FontDebugInfo>& infos) {
    { manager.findFont(id, pixel_size) } -> std::same_as<FontHandle>;
    { manager.loadFont(id, pixel_size, path) } -> std::same_as<FontHandle>;
    manager.unloadFont(id, pixel_size);
    manager.clearFonts();
    manager.collectDebugInfo(infos);
};

TEST(ResourceManagerSubmanagerVisibilityTest, ManagerBusinessApisArePubliclyAccessible) {
    static_assert(TextureManagerApiVisible<TextureManager>);
    static_assert(AudioManagerApiVisible<AudioManager>);
    static_assert(FontManagerApiVisible<FontManager>);
    SUCCEED();
}

} // namespace
} // namespace engine::resource
