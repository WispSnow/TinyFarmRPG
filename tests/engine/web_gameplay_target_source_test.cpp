// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::filesystem::path projectPath(const char* relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

[[nodiscard]] std::string readProjectFile(const char* relative_path) {
    const std::filesystem::path path = projectPath(relative_path);
    EXPECT_TRUE(std::filesystem::exists(path)) << path;
    return test_source_utils::readTextFile(path);
}

TEST(WebGameplayTargetSourceTest, ReusesSharedSdlCallbackMain) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string main_source = readProjectFile("src/main.cpp");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(main_source.empty());

    EXPECT_FALSE(std::filesystem::exists(projectPath("src/web/web_game_main.cpp")));
    EXPECT_NE(root_cmake.find("if(TF_BUILD_WEB AND TF_BUILD_WEB_SKELETON)"), std::string::npos);
    EXPECT_NE(root_cmake.find("tf_configure_web_executable(${TARGET})"), std::string::npos);
    EXPECT_NE(root_cmake.find("option(TF_WEB_DIRECT_MAP_BOOT"), std::string::npos);
    EXPECT_NE(root_cmake.find("set(TF_DEFAULT_WEB_DIRECT_MAP_BOOT OFF)"), std::string::npos);
    EXPECT_NE(root_cmake.find("add_compile_definitions(TF_WEB_DIRECT_MAP_BOOT)"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_MAIN_USE_CALLBACKS"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppInit"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppIterate"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppEvent"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppQuit"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebRuntimeOwnsExceptionAndLinkPolicy) {
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");

    ASSERT_FALSE(runtime_cmake.empty());

    EXPECT_NE(runtime_cmake.find("-fno-exceptions"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("SPDLOG_NO_EXCEPTIONS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("JSON_NOEXCEPTION"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("SOL_NO_EXCEPTIONS=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sDISABLE_EXCEPTION_CATCHING=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sMIN_WEBGL_VERSION=2"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sMAX_WEBGL_VERSION=2"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sFORCE_FILESYSTEM=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-lidbfs.js"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("if(TF_WEB_ENABLE_PTHREADS)"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebDependenciesAvoidDesktopGlAndImageLibraries) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string dependencies_cmake = readProjectFile("cmake/WebDependencies.cmake");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(dependencies_cmake.empty());

    const std::size_t desktop_link_guard = root_cmake.find("if(NOT TF_BUILD_WEB)");
    const std::size_t sdl_image_link = root_cmake.find("SDL3_image::SDL3_image");
    const std::size_t opengl_link = root_cmake.find("OpenGL::GL");
    const std::size_t glad_link = root_cmake.find("glad");

    ASSERT_NE(desktop_link_guard, std::string::npos);
    ASSERT_NE(sdl_image_link, std::string::npos);
    ASSERT_NE(opengl_link, std::string::npos);
    ASSERT_NE(glad_link, std::string::npos);
    EXPECT_LT(desktop_link_guard, sdl_image_link);
    EXPECT_LT(desktop_link_guard, opengl_link);
    EXPECT_LT(desktop_link_guard, glad_link);

    EXPECT_NE(dependencies_cmake.find("add_library(tf_web_sdl3 INTERFACE)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("add_library(SDL3::SDL3 ALIAS tf_web_sdl3)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("-sUSE_SDL=3"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("function(tf_web_add_lua)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("function(tf_web_add_sol2)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("SPDLOG_NO_EXCEPTIONS"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("function(tf_web_configure_thread_stubs)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("CMAKE_HAVE_LIBC_PTHREAD TRUE"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("CMAKE_THREAD_LIBS_INIT \"\""), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebGlPlatformGuardsSrgbAndDepthClear) {
    const std::string platform_header = readProjectFile("src/engine/platform/gl_platform.h");
    const std::string renderer_header = readProjectFile("src/engine/render/opengl/gl_renderer.h");
    const std::string renderer_source = readProjectFile("src/engine/render/opengl/gl_renderer.cpp");

    ASSERT_FALSE(platform_header.empty());
    ASSERT_FALSE(renderer_header.empty());
    ASSERT_FALSE(renderer_source.empty());

    EXPECT_NE(platform_header.find("TF_GL_PLATFORM_WEBGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsDefaultFramebufferSrgb = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsFloatColorFramebuffers = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsLinearFloatFiltering = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepthf(depth);"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepth(static_cast<GLdouble>(depth));"), std::string::npos);
    EXPECT_NE(renderer_header.find("RenderCapabilitySnapshot"), std::string::npos);
    EXPECT_NE(renderer_header.find("getRenderCapabilitySnapshot"), std::string::npos);
    EXPECT_NE(renderer_source.find("engine::platform::gl::kSupportsDefaultFramebufferSrgb"), std::string::npos);
    EXPECT_NE(renderer_source.find("detectRenderCapabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("queryWebGlExtension(\"EXT_color_buffer_float\")"), std::string::npos);
    EXPECT_NE(renderer_source.find("queryWebGlExtension(\"OES_texture_float_linear\")"), std::string::npos);
    EXPECT_NE(renderer_source.find("probeColorRenderableFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT)"),
              std::string::npos);
    EXPECT_EQ(renderer_source.find("kEnableHdrPostProcessingByDefault"), std::string::npos);
    EXPECT_NE(renderer_source.find("bloom_enabled_ && bloom_pass_ && emissive_pass_"), std::string::npos);
    EXPECT_NE(renderer_source.find("TinyFarmRPGWebReleaseDiagnostics"), std::string::npos);
    EXPECT_NE(renderer_source.find("GLRenderer: Web release render capabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("engine::platform::gl::clearDepth(1.0f);"), std::string::npos);
    EXPECT_EQ(renderer_source.find("glClearDepth(1.0);"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, DirectMapBootBypassesTitleAndRmlUiRuntime) {
    const std::string game_entry_source = readProjectFile("src/game/game_entry.cpp");
    const std::string game_app_source = readProjectFile("src/engine/core/game_app.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_loading_source = readProjectFile("src/game/world/map_loading_settings.cpp");

    ASSERT_FALSE(game_entry_source.empty());
    ASSERT_FALSE(game_app_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(map_loading_source.empty());

    EXPECT_NE(game_entry_source.find("#ifdef TF_WEB_DIRECT_MAP_BOOT"), std::string::npos);
    EXPECT_NE(game_entry_source.find("#include \"game/scene/game_scene.h\""), std::string::npos);
    EXPECT_NE(game_entry_source.find("std::make_unique<game::scene::GameScene>"), std::string::npos);
    EXPECT_NE(game_entry_source.find("Web direct map boot: pushing GameScene at home_exterior."), std::string::npos);
    EXPECT_NE(game_entry_source.find("std::make_unique<game::scene::TitleScene>"), std::string::npos);

    EXPECT_NE(game_app_source.find("constexpr bool kEnableRmlUiRuntime = false;"), std::string::npos);
    EXPECT_NE(game_app_source.find("if constexpr (kEnableRmlUiRuntime)"), std::string::npos);
    EXPECT_NE(game_app_source.find("GameApp: RmlUi runtime disabled for Web direct map boot."), std::string::npos);

    EXPECT_NE(game_scene_source.find("GameScene: skipping RmlUi HUD for Web direct map boot."), std::string::npos);
    EXPECT_NE(game_scene_source.find("inventory menu disabled because RmlUiRuntime is unavailable"), std::string::npos);
    EXPECT_NE(game_scene_source.find("pause menu disabled because RmlUiRuntime is unavailable"), std::string::npos);

    EXPECT_NE(map_loading_source.find("#ifdef TF_BUILD_WEB"), std::string::npos);
    EXPECT_NE(map_loading_source.find("settings.preload_mode = MapPreloadMode::Off;"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, TitleBootRestoresRmlUiAndPhase11UiResources) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string game_entry_source = readProjectFile("src/game/game_entry.cpp");
    const std::string game_app_source = readProjectFile("src/engine/core/game_app.cpp");
    const std::string asset_audit_source = readProjectFile("tools/asset_audit/audit_assets.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");
    const std::string preload_manifest = readProjectFile("manifests/assets/web-release-full.args");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(game_entry_source.empty());
    ASSERT_FALSE(game_app_source.empty());
    ASSERT_FALSE(asset_audit_source.empty());
    ASSERT_FALSE(release_validator_source.empty());
    ASSERT_FALSE(preload_manifest.empty());

    EXPECT_NE(root_cmake.find("set(TF_DEFAULT_WEB_DIRECT_MAP_BOOT OFF)"), std::string::npos);
    EXPECT_NE(game_entry_source.find("#else"), std::string::npos);
    EXPECT_NE(game_entry_source.find("std::make_unique<game::scene::TitleScene>"), std::string::npos);
    EXPECT_NE(game_app_source.find("constexpr bool kEnableRmlUiRuntime = true;"), std::string::npos);

    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/appearance_customize.\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/inventory_menu.\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/pause_menu.\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/save_slot_select.\""), std::string::npos);

    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/appearance_customize.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/inventory_menu.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/inventory_menu.rcss\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/pause_menu.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/save_slot_select.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("build_dir / \"TinyFarmRPG-Web-preload-root\""), std::string::npos);

    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/title.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/appearance_customize.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/inventory_menu.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/inventory_menu.rcss"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/pause_menu.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/save_slot_select.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/hud/hotbar.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/fonts/VonwaonBitmap-16px.ttf"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/fonts/LXGWBright-Regular.ttf"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase12InitializesPersistentStorageAndUnlocksWebAudioFromUserGesture) {
    const std::string game_app_header = readProjectFile("src/engine/core/game_app.h");
    const std::string game_app_source = readProjectFile("src/engine/core/game_app.cpp");
    const std::string audio_header = readProjectFile("src/engine/audio/audio_player.h");
    const std::string audio_source = readProjectFile("src/engine/audio/audio_player.cpp");
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");

    ASSERT_FALSE(game_app_header.empty());
    ASSERT_FALSE(game_app_source.empty());
    ASSERT_FALSE(audio_header.empty());
    ASSERT_FALSE(audio_source.empty());
    ASSERT_FALSE(runtime_cmake.empty());

    EXPECT_NE(runtime_cmake.find("-lidbfs.js"), std::string::npos);
    EXPECT_NE(game_app_header.find("initPersistentStorage"), std::string::npos);
    EXPECT_NE(game_app_source.find("syncPersistentStorageFromBrowser"), std::string::npos);
    EXPECT_NE(game_app_source.find("tryStartAudioFromUserGesture(event);"), std::string::npos);
    EXPECT_NE(game_app_source.find("SDL_EVENT_MOUSE_BUTTON_DOWN"), std::string::npos);
    EXPECT_NE(game_app_source.find("SDL_EVENT_KEY_DOWN"), std::string::npos);
    EXPECT_NE(game_app_source.find("startPlaybackAfterUserGesture"), std::string::npos);

    EXPECT_NE(audio_header.find("startPlaybackAfterUserGesture"), std::string::npos);
    EXPECT_NE(audio_header.find("isPlaybackReady"), std::string::npos);
    EXPECT_NE(audio_source.find("config.noAutoStart = MA_TRUE"), std::string::npos);
    EXPECT_NE(audio_source.find("ma_engine_start(&engine_)"), std::string::npos);
    EXPECT_NE(audio_source.find("pending_music_"), std::string::npos);
    EXPECT_NE(audio_source.find("playPendingMusicAfterPlaybackStart"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase12AsyncSaveCompletionWaitsForPersistentStorageSync) {
    const std::string save_service_source = readProjectFile("src/game/save/save_service.cpp");
    const std::string persistent_storage_source = readProjectFile("src/engine/platform/web_persistent_storage.cpp");

    ASSERT_FALSE(save_service_source.empty());
    ASSERT_FALSE(persistent_storage_source.empty());

    EXPECT_NE(save_service_source.find("#include \"engine/platform/web_persistent_storage.h\""), std::string::npos);
    EXPECT_NE(save_service_source.find("syncPersistentStorageToBrowser(&onAsyncSavePersistentSync"), std::string::npos);
    EXPECT_NE(save_service_source.find("appendPersistentSyncError"), std::string::npos);
    EXPECT_NE(save_service_source.find("Web persistent storage sync completed after async save"), std::string::npos);
    EXPECT_NE(save_service_source.find("completion->save_in_progress->store(false"), std::string::npos);
    EXPECT_NE(save_service_source.find("enqueueAsyncSaveCompleted(*completion->main_thread_queue"), std::string::npos);
    EXPECT_NE(save_service_source.find("syncPersistentStorageToBrowser(&onDirectSavePersistentSync"), std::string::npos);

    EXPECT_NE(persistent_storage_source.find("bool in_progress{false}"), std::string::npos);
    EXPECT_NE(persistent_storage_source.find("TinyFarmRPG persistent FS sync is already in progress."), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase12PreloadsFullAudioCoreResources) {
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");
    const std::string preload_manifest = readProjectFile("manifests/assets/web-release-full.args");

    ASSERT_FALSE(release_validator_source.empty());
    ASSERT_FALSE(preload_manifest.empty());

    EXPECT_NE(release_validator_source.find("\"assets/audio/pop.mp3\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/01_spring_journey.ogg\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/02_spring_fairy_tale.ogg\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/shovel-stab.wav\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/chop-wood.wav\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/Damage1.ogg\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"config/audio.json\""), std::string::npos);

    EXPECT_NE(preload_manifest.find("assets/audio/pop.mp3"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/01_spring_journey.ogg"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/02_spring_fairy_tale.ogg"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/shovel-stab.wav"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/chop-wood.wav"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/Damage1.ogg"), std::string::npos);
    EXPECT_NE(preload_manifest.find("config/audio.json"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase13RuntimePackagePipelineIsPresent) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");
    const std::string src_cmake = readProjectFile("src/CMakeLists.txt");
    const std::string package_loader_header = readProjectFile("src/engine/platform/web_asset_package.h");
    const std::string package_loader_source = readProjectFile("src/engine/platform/web_asset_package.cpp");
    const std::string package_registry_header = readProjectFile("src/engine/platform/web_asset_package_registry.h");
    const std::string package_registry_source = readProjectFile("src/engine/platform/web_asset_package_registry.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_manager_source = readProjectFile("src/game/world/map_manager.cpp");
    const std::string package_tool = readProjectFile("tools/web_release/package_web_assets.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(runtime_cmake.empty());
    ASSERT_FALSE(src_cmake.empty());
    ASSERT_FALSE(package_loader_header.empty());
    ASSERT_FALSE(package_loader_source.empty());
    ASSERT_FALSE(package_registry_header.empty());
    ASSERT_FALSE(package_registry_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(map_manager_source.empty());
    ASSERT_FALSE(package_tool.empty());
    ASSERT_FALSE(release_validator_source.empty());

    EXPECT_NE(root_cmake.find("option(TF_WEB_ENABLE_RUNTIME_PACKAGES"), std::string::npos);
    EXPECT_NE(root_cmake.find("add_compile_definitions(TF_WEB_ENABLE_RUNTIME_PACKAGES)"), std::string::npos);

    EXPECT_NE(runtime_cmake.find("function(tf_configure_web_runtime_packages"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("package_web_assets.py"), std::string::npos);
    EXPECT_EQ(runtime_cmake.find("-sFETCH=1"), std::string::npos);

    EXPECT_NE(src_cmake.find("engine/platform/web_asset_package.cpp"), std::string::npos);
    EXPECT_NE(src_cmake.find("engine/platform/web_asset_package_registry.cpp"), std::string::npos);
    EXPECT_NE(package_loader_header.find("loadAssetPackage"), std::string::npos);
    EXPECT_NE(package_loader_source.find("XMLHttpRequest"), std::string::npos);
    EXPECT_NE(package_loader_source.find("overrideMimeType"), std::string::npos);
    EXPECT_EQ(package_loader_source.find("emscripten_fetch"), std::string::npos);
    EXPECT_NE(package_loader_source.find("PACKAGE_MAGIC"), std::string::npos);
    EXPECT_NE(package_loader_source.find("'T', 'F', 'P', 'K'"), std::string::npos);
    EXPECT_NE(package_loader_source.find("std::filesystem::create_directories"), std::string::npos);
    EXPECT_NE(package_registry_header.find("loadPackage"), std::string::npos);
    EXPECT_NE(package_registry_header.find("loadGroup"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_AUDIO_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_TOWN_MAP"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_BATTLE_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_VFX_CORE"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/shared-ui.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/rpg-core.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/home-map.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/town-map.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/battle-core.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/vfx-core.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("web-packages/audio-core.tfpack"), std::string::npos);
    EXPECT_NE(package_registry_source.find("lastPackageError"), std::string::npos);

    EXPECT_NE(game_scene_source.find("ensureWebGameplayPackages"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_HOME_MAP"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_SHARED_UI"), std::string::npos);
    EXPECT_NE(map_manager_source.find("ensureWebMapPackage"), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_HOME_MAP"), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_TOWN_MAP"), std::string::npos);

    EXPECT_NE(package_tool.find("custom_sync_xhr_fs_writefile"), std::string::npos);
    EXPECT_NE(package_tool.find("\"boot\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"shared-ui\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"rpg-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"home-map\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"town-map\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"battle-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"vfx-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"audio-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("PACKAGE_DEPENDENCIES"), std::string::npos);
    EXPECT_NE(package_tool.find("write_tfpack"), std::string::npos);

    EXPECT_NE(release_validator_source.find("validate_runtime_packages"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_SHARED_UI_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_RPG_CORE_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_HOME_MAP_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_TOWN_MAP_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_BATTLE_CORE_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_VFX_CORE_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_AUDIO_CORE_PACKAGE_PATHS"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase15BootOnlyPreloadCutoverIsPresent) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");
    const std::string package_tool = readProjectFile("tools/web_release/package_web_assets.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");
    const std::string release_manifest = readProjectFile("manifests/assets/web-release-full.args");
    const std::string boot_manifest = readProjectFile("manifests/assets/web-release-boot.args");
    const std::string title_scene_source = readProjectFile("src/game/scene/title_scene.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string runtime_assembler_header = readProjectFile("src/game/runtime/game_runtime_assembler.h");
    const std::string runtime_service_factory = readProjectFile("src/game/runtime/runtime_service_factory.cpp");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(runtime_cmake.empty());
    ASSERT_FALSE(package_tool.empty());
    ASSERT_FALSE(release_validator_source.empty());
    ASSERT_FALSE(release_manifest.empty());
    ASSERT_FALSE(boot_manifest.empty());
    ASSERT_FALSE(title_scene_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(runtime_assembler_header.empty());
    ASSERT_FALSE(runtime_service_factory.empty());

    EXPECT_NE(root_cmake.find("option(TF_WEB_BOOT_ONLY_PRELOAD"), std::string::npos);
    EXPECT_NE(root_cmake.find("set(TF_DEFAULT_WEB_BOOT_ONLY_PRELOAD ON)"), std::string::npos);
    EXPECT_NE(root_cmake.find("TF_WEB_BOOT_ONLY_PRELOAD=ON requires TF_WEB_ENABLE_RUNTIME_PACKAGES=ON"),
              std::string::npos);

    EXPECT_NE(runtime_cmake.find("TF_WEB_FULL_PRELOAD_ARGS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("manifests/assets/web-release-full.args"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("TF_WEB_BOOT_PRELOAD_ARGS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("manifests/assets/web-release-boot.args"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("TF_WEB_GENERATED_BOOT_PRELOAD_ARGS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("TF_WEB_PRELOAD_ARGS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("tf_target_web_preload(${TARGET_NAME} \"${TF_WEB_PRELOAD_ARGS}\")"),
              std::string::npos);

    EXPECT_NE(package_tool.find("\"--skip-artifacts\""), std::string::npos);
    EXPECT_NE(package_tool.find("web-release-full.args"), std::string::npos);

    EXPECT_NE(release_validator_source.find("REQUIRED_BOOT_PRELOAD_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("FORBIDDEN_BOOT_PRELOAD_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("BOOT_DATA_BUDGET_BYTES"), std::string::npos);
    EXPECT_NE(release_validator_source.find("validate_full_manifest_budget"), std::string::npos);
    EXPECT_NE(release_validator_source.find("validate_boot_preload_budget"), std::string::npos);
    EXPECT_NE(release_validator_source.find("TF_WEB_FULL_PRELOAD_ARGS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("TF_WEB_BOOT_ONLY_PRELOAD"), std::string::npos);

    EXPECT_NE(title_scene_source.find("ensureWebSharedUiPackage"), std::string::npos);
    EXPECT_NE(title_scene_source.find("PACKAGE_SHARED_UI"), std::string::npos);
    EXPECT_NE(game_scene_source.find("std::holds_alternative<LoadGameOptions>(launch_)"), std::string::npos);
    EXPECT_NE(runtime_assembler_header.find("bool load_initial_map{true}"), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("RuntimeServiceFactory: initial map load skipped."), std::string::npos);

    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/title.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("assets/maps/home_exterior.tmj"), std::string::npos);
    EXPECT_NE(release_manifest.find("assets/audio/01_spring_journey.ogg"), std::string::npos);
    EXPECT_NE(boot_manifest.find("ui/rmlui/scenes/title.rml"), std::string::npos);
    EXPECT_NE(boot_manifest.find("ui/rmlui/scenes/title_widgets.rcss"), std::string::npos);
    EXPECT_NE(boot_manifest.find("ui/rmlui/theme/base.rcss"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/farm-rpg/UI/button.png"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/i18n/languages.json"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/i18n/en-US.json"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/i18n/zh-Hans.json"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("assets/maps/home_exterior.tmj"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("assets/audio/01_spring_journey.ogg"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("ui/rmlui/scenes/appearance_customize.rml"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("ui/rmlui/theme/spritesheet.rcss"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase14ChromiumSmokePipelineIsPresent) {
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_server = readProjectFile("tools/web_release/serve_web_release.py");

    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_server.empty());

    EXPECT_NE(web_smoke.find("configure_web_build"), std::string::npos);
    EXPECT_NE(web_smoke.find("validate_web_release.py"), std::string::npos);
    EXPECT_NE(web_smoke.find("remote-debugging-port"), std::string::npos);
    EXPECT_NE(web_smoke.find("--headed"), std::string::npos);
    EXPECT_NE(web_smoke.find("--no-sandbox"), std::string::npos);
    EXPECT_NE(web_smoke.find("--use-angle=swiftshader"), std::string::npos);
    EXPECT_NE(web_smoke.find("Runtime.consoleAPICalled"), std::string::npos);
    EXPECT_NE(web_smoke.find("Network.responseReceived"), std::string::npos);
    EXPECT_NE(web_smoke.find("click_logical"), std::string::npos);
    EXPECT_NE(web_smoke.find("SAVE_PATH = \"/persistent/saves/slot0.json\""), std::string::npos);
    EXPECT_NE(web_smoke.find("Player did not move far enough"), std::string::npos);
    EXPECT_NE(web_smoke.find("SaveService: 已载入存档 'home_exterior'"), std::string::npos);
    EXPECT_NE(web_smoke.find("shared-ui.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("rpg-core.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("home-map.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("audio-core.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("WebAssetPackageRegistry: package 'audio-core' ready"), std::string::npos);
    EXPECT_NE(web_smoke.find("Single-thread preview must not require COOP/COEP headers"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_home_round_trip"), std::string::npos);
    EXPECT_NE(web_smoke.find("trigger_merchant_dialogue"), std::string::npos);
    EXPECT_NE(web_smoke.find("covered_flows"), std::string::npos);

    EXPECT_NE(release_server.find("\".wasm\": \"application/wasm\""), std::string::npos);
    EXPECT_NE(release_server.find("\".tfpack\": \"application/octet-stream\""), std::string::npos);
    EXPECT_NE(release_server.find("Cache-Control"), std::string::npos);
    EXPECT_NE(release_server.find("Cross-Origin-Opener-Policy"), std::string::npos);
    EXPECT_NE(release_server.find("Cross-Origin-Embedder-Policy"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase16RuntimePackageRegistryAndAudioGateArePresent) {
    const std::string package_registry_header = readProjectFile("src/engine/platform/web_asset_package_registry.h");
    const std::string package_registry_source = readProjectFile("src/engine/platform/web_asset_package_registry.cpp");
    const std::string game_app_header = readProjectFile("src/engine/core/game_app.h");
    const std::string game_app_source = readProjectFile("src/engine/core/game_app.cpp");
    const std::string resource_manager_header = readProjectFile("src/engine/resource/resource_manager.h");
    const std::string resource_manager_source = readProjectFile("src/engine/resource/resource_manager.cpp");
    const std::string title_scene_source = readProjectFile("src/game/scene/title_scene.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_manager_source = readProjectFile("src/game/world/map_manager.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string boot_manifest = readProjectFile("manifests/assets/web-release-boot.args");

    ASSERT_FALSE(package_registry_header.empty());
    ASSERT_FALSE(package_registry_source.empty());
    ASSERT_FALSE(game_app_header.empty());
    ASSERT_FALSE(game_app_source.empty());
    ASSERT_FALSE(resource_manager_header.empty());
    ASSERT_FALSE(resource_manager_source.empty());
    ASSERT_FALSE(title_scene_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(map_manager_source.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(boot_manifest.empty());

    EXPECT_NE(package_registry_header.find("PACKAGE_SHARED_UI"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_HOME_MAP"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_TOWN_MAP"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_BATTLE_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_VFX_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_AUDIO_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("loadGroup"), std::string::npos);
    EXPECT_NE(package_registry_header.find("packageFiles"), std::string::npos);
    EXPECT_NE(package_registry_header.find("packageBytes"), std::string::npos);
    EXPECT_NE(package_registry_source.find("WebAssetPackageRegistry: loading package"), std::string::npos);
    EXPECT_NE(package_registry_source.find("WebAssetPackageRegistry: package '{}' ready"), std::string::npos);
    EXPECT_NE(package_registry_source.find("lastPackageError"), std::string::npos);
    EXPECT_NE(package_registry_source.find("std::array<PackageDefinition, 7>"), std::string::npos);
    EXPECT_EQ(package_registry_source.find("std::array<PackageDefinition, 3>"), std::string::npos);

    EXPECT_NE(title_scene_source.find("loadPackage(engine::platform::web::PACKAGE_SHARED_UI)"), std::string::npos);
    EXPECT_NE(game_scene_source.find("loadGroup({"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_SHARED_UI"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_HOME_MAP"), std::string::npos);
    EXPECT_NE(map_manager_source.find("loadGroup({"), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_HOME_MAP"), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_TOWN_MAP"), std::string::npos);
    EXPECT_EQ(title_scene_source.find("web-packages/shared-ui.tfpack"), std::string::npos);
    EXPECT_EQ(game_scene_source.find("web-packages/home-map.tfpack"), std::string::npos);
    EXPECT_EQ(map_manager_source.find("web-packages/home-map.tfpack"), std::string::npos);

    EXPECT_NE(game_app_header.find("web_audio_core_preloaded_"), std::string::npos);
    EXPECT_NE(game_app_source.find("PACKAGE_AUDIO_CORE"), std::string::npos);
    EXPECT_NE(game_app_source.find("preloadRegisteredAudioResources"), std::string::npos);
    EXPECT_NE(resource_manager_header.find("preloadRegisteredAudioResources"), std::string::npos);
    EXPECT_NE(resource_manager_source.find("registered audio preload complete"), std::string::npos);
    EXPECT_NE(resource_manager_source.find("已注册但暂不解码"), std::string::npos);

    EXPECT_NE(web_smoke.find("shared-ui.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("rpg-core.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("home-map.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("audio-core.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("Package responses missing from smoke"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("web-packages/audio-core.tfpack"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("assets/audio/01_spring_journey.ogg"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase17GameplayCoverageSmokeIsPresent) {
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string audit_tool = readProjectFile("tools/web_release/audit_web_resource_coverage.py");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_transition_source = readProjectFile("src/game/system/map_transition_system.cpp");
    const std::string dialogue_presentation_source = readProjectFile("src/game/ui/dialogue_presentation_controller.cpp");
    const std::string input_routing_source = readProjectFile("src/engine/input/input_event_routing.cpp");

    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(audit_tool.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(map_transition_source.empty());
    ASSERT_FALSE(dialogue_presentation_source.empty());
    ASSERT_FALSE(input_routing_source.empty());

    EXPECT_NE(web_smoke.find("phase17-home-interior.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("phase17-home-exterior-return.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("phase17-inventory-open.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("phase17-pause-open.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("phase17-merchant-approach.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("phase17-merchant-dialogue.png"), std::string::npos);
    EXPECT_NE(web_smoke.find("package_web_assets(root, build_dir)"), std::string::npos);
    EXPECT_NE(web_smoke.find("TinyFarmRPGSmokeState"), std::string::npos);
    EXPECT_NE(web_smoke.find("move_player_to"), std::string::npos);
    EXPECT_NE(web_smoke.find("home_exterior_to_home_interior_round_trip"), std::string::npos);
    EXPECT_NE(web_smoke.find("primary_tool_action"), std::string::npos);
    EXPECT_NE(web_smoke.find("scripted_merchant_dialogue"), std::string::npos);

    EXPECT_NE(game_scene_source.find("GameScene: inventory menu opened."), std::string::npos);
    EXPECT_NE(game_scene_source.find("GameScene: hotbar toggle accepted."), std::string::npos);
    EXPECT_NE(game_scene_source.find("GameScene: pause menu opened."), std::string::npos);
    EXPECT_NE(game_scene_source.find("GameScene: gameplay ready."), std::string::npos);
    EXPECT_NE(game_scene_source.find("TinyFarmRPGSmokeState"), std::string::npos);
    EXPECT_NE(map_transition_source.find("MapTransitionSystem: map transition"), std::string::npos);
    EXPECT_NE(dialogue_presentation_source.find("DialoguePresentationController: conversation dialogue shown."), std::string::npos);
    EXPECT_NE(input_routing_source.find("SDL_EVENT_KEY_DOWN && current_context == InputContextId::Gameplay"), std::string::npos);
    EXPECT_NE(input_routing_source.find("SDL_EVENT_MOUSE_BUTTON_DOWN && current_context == InputContextId::Gameplay"), std::string::npos);

    EXPECT_NE(audit_tool.find("REQUIRED_GAMEPLAY_SURFACES"), std::string::npos);
    EXPECT_NE(audit_tool.find("ui/rmlui/scenes/inventory_menu.rml"), std::string::npos);
    EXPECT_NE(audit_tool.find("home_interior"), std::string::npos);
    EXPECT_NE(audit_tool.find("scripted interactions"), std::string::npos);
    EXPECT_NE(audit_tool.find("missing_required_paths"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase18RenderAudioVfxDiagnosticsArePresent) {
    const std::string renderer_header = readProjectFile("src/engine/render/opengl/gl_renderer.h");
    const std::string renderer_source = readProjectFile("src/engine/render/opengl/gl_renderer.cpp");
    const std::string resource_manager_source = readProjectFile("src/engine/resource/resource_manager.cpp");
    const std::string runtime_service_factory = readProjectFile("src/game/runtime/runtime_service_factory.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");

    ASSERT_FALSE(renderer_header.empty());
    ASSERT_FALSE(renderer_source.empty());
    ASSERT_FALSE(resource_manager_source.empty());
    ASSERT_FALSE(runtime_service_factory.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_validator_source.empty());

    EXPECT_NE(renderer_header.find("RenderCapabilitySnapshot"), std::string::npos);
    EXPECT_NE(renderer_header.find("default_framebuffer_srgb"), std::string::npos);
    EXPECT_NE(renderer_header.find("float_color_framebuffers"), std::string::npos);
    EXPECT_NE(renderer_header.find("rgba16f_color_renderable"), std::string::npos);
    EXPECT_NE(renderer_header.find("linear_float_filtering"), std::string::npos);
    EXPECT_NE(renderer_header.find("hdr_fallback_reason"), std::string::npos);
    EXPECT_NE(renderer_source.find("captureRenderCapabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("detectRenderCapabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("publishRenderCapabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("TinyFarmRPGWebReleaseDiagnostics"), std::string::npos);
    EXPECT_NE(renderer_source.find("GLRenderer: Web release render capabilities"), std::string::npos);

    EXPECT_NE(runtime_service_factory.find("Web release VFX policy"), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("publishWebVfxDiagnostics"), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("backend=null_vfx_backend status=deferred"), std::string::npos);
    EXPECT_NE(resource_manager_source.find("Web audio release policy"), std::string::npos);
    EXPECT_NE(resource_manager_source.find("failed=0"), std::string::npos);

    EXPECT_NE(web_smoke.find("read_render_capabilities"), std::string::npos);
    EXPECT_NE(web_smoke.find("validate_render_capabilities"), std::string::npos);
    EXPECT_NE(web_smoke.find("wait_for_render_postprocessing_activity"), std::string::npos);
    EXPECT_NE(web_smoke.find("collect_webgl_error_logs"), std::string::npos);
    EXPECT_NE(web_smoke.find("performance_budget"), std::string::npos);
    EXPECT_NE(web_smoke.find("vfx_policy"), std::string::npos);
    EXPECT_NE(web_smoke.find("diagnostics.vfx.backend expected effekseer"), std::string::npos);
    EXPECT_NE(web_smoke.find("audio_policy"), std::string::npos);

    EXPECT_NE(release_validator_source.find("Web audio release policy"), std::string::npos);
    EXPECT_NE(release_validator_source.find("Web release VFX policy"), std::string::npos);
    EXPECT_NE(release_validator_source.find("render_capability_gate"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase24EffekseerWebBackendIsEnabled) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string effekseer_cmake = readProjectFile("cmake/EffekseerDependencies.cmake");
    const std::string effekseer_backend = readProjectFile("src/engine/vfx/effekseer_backend.cpp");
    const std::string effekseer_gl_extension =
        readProjectFile("external/Effekseer-1.7.3.0/Dev/Cpp/EffekseerRendererGL/EffekseerRenderer/EffekseerRendererGL.GLExtension.cpp");
    const std::string runtime_service_factory = readProjectFile("src/game/runtime/runtime_service_factory.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(effekseer_cmake.empty());
    ASSERT_FALSE(effekseer_backend.empty());
    ASSERT_FALSE(effekseer_gl_extension.empty());
    ASSERT_FALSE(runtime_service_factory.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_validator_source.empty());

    EXPECT_NE(root_cmake.find("set(TF_DEFAULT_ENABLE_EFFEKSEER ON)"), std::string::npos);
    EXPECT_NE(root_cmake.find("set(ENABLE_EFFEKSEER ON CACHE BOOL \"Web full RPG release requires Effekseer VFX backend\" FORCE)"),
              std::string::npos);
    EXPECT_NE(root_cmake.find("include(cmake/EffekseerDependencies.cmake)"), std::string::npos);
    EXPECT_NE(root_cmake.find("setup_effekseer_dependencies()"), std::string::npos);

    EXPECT_NE(effekseer_cmake.find("set(USE_OPENGLES3 ON CACHE BOOL"), std::string::npos);
    EXPECT_NE(effekseer_cmake.find("__EFFEKSEER_RENDERER_GLES3__"), std::string::npos);

    EXPECT_NE(effekseer_backend.find("OpenGLDeviceType::OpenGLES3"), std::string::npos);
    EXPECT_NE(effekseer_backend.find("OpenGLDeviceType::OpenGL3"), std::string::npos);

    EXPECT_NE(effekseer_gl_extension.find("#if defined(__EMSCRIPTEN__)\n\t\tg_isSurrpotedBufferRange = false;"), std::string::npos);
    EXPECT_NE(effekseer_gl_extension.find("#elif defined(__EMSCRIPTEN__)\n\treturn nullptr;"), std::string::npos);
    EXPECT_NE(effekseer_gl_extension.find("#elif defined(__EMSCRIPTEN__)\n\treturn GL_FALSE;"), std::string::npos);

    EXPECT_NE(runtime_service_factory.find("publishWebVfxDiagnostics("), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("backend ? \"effekseer\" : \"null_vfx_backend\""), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("backend ? \"enabled\" : \"fallback\""), std::string::npos);

    EXPECT_NE(release_validator_source.find("\"ENABLE_EFFEKSEER\": True"), std::string::npos);
    EXPECT_NE(web_smoke.find("diagnostics.vfx.effekseerEnabled expected true"), std::string::npos);
    EXPECT_NE(web_smoke.find("diagnostics.vfx.backend expected effekseer"), std::string::npos);
    EXPECT_NE(web_smoke.find("diagnostics.vfx.status expected enabled"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase25FullRpgBattleFlowIsReachableOnWeb) {
    const std::string home_map = readProjectFile("assets/maps/home_exterior.tmj");
    const std::string town_map = readProjectFile("assets/maps/town.tmj");
    const std::string game_scene = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string battle_scene_header = readProjectFile("src/game/scene/battle_scene.h");
    const std::string battle_scene_source = readProjectFile("src/game/scene/battle_scene.cpp");
    const std::string sprite_batch_header = readProjectFile("src/engine/render/opengl/sprite_batch.h");
    const std::string texture_shader = readProjectFile("assets/shaders/texture.frag");
    const std::string text_renderer = readProjectFile("src/engine/render/text_renderer.cpp");
    const std::string font_manager = readProjectFile("src/engine/resource/font_manager.cpp");
    const std::string encounter_system = readProjectFile("src/game/system/enemy_encounter_system.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string boot_manifest = readProjectFile("manifests/assets/web-release-boot.args");

    ASSERT_FALSE(home_map.empty());
    ASSERT_FALSE(town_map.empty());
    ASSERT_FALSE(game_scene.empty());
    ASSERT_FALSE(battle_scene_header.empty());
    ASSERT_FALSE(battle_scene_source.empty());
    ASSERT_FALSE(sprite_batch_header.empty());
    ASSERT_FALSE(texture_shader.empty());
    ASSERT_FALSE(text_renderer.empty());
    ASSERT_FALSE(font_manager.empty());
    ASSERT_FALSE(encounter_system.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(boot_manifest.empty());

    EXPECT_NE(home_map.find("\"name\":\"town_path\""), std::string::npos);
    EXPECT_NE(home_map.find("\"value\":\"town\""), std::string::npos);
    EXPECT_NE(town_map.find("\"name\":\"home_path\""), std::string::npos);
    EXPECT_NE(town_map.find("\"value\":\"home_exterior\""), std::string::npos);
    EXPECT_NE(town_map.find("\"battle_troop_id\""), std::string::npos);
    EXPECT_NE(town_map.find("\"value\":\"troop.slime\""), std::string::npos);
    EXPECT_NE(town_map.find("\"value\":\"troop.slime_single\""), std::string::npos);

    EXPECT_NE(game_scene.find("ensureWebBattlePackages"), std::string::npos);
    EXPECT_NE(game_scene.find("PACKAGE_BATTLE_CORE"), std::string::npos);
    EXPECT_NE(game_scene.find("PACKAGE_VFX_CORE"), std::string::npos);
    EXPECT_NE(game_scene.find("Web battle packages failed to load"), std::string::npos);
    EXPECT_NE(game_scene.find("availableEncounterCount"), std::string::npos);
    EXPECT_NE(game_scene.find("gameplay.encounters"), std::string::npos);
    EXPECT_NE(encounter_system.find("EnemyEncounterSystem: triggering battle"), std::string::npos);

    EXPECT_NE(battle_scene_header.find("publishWebBattleDiagnostics"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("TinyFarmRPGWebReleaseDiagnostics"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("battle.currentScene = \"BattleScene\""), std::string::npos);
    EXPECT_NE(battle_scene_source.find("const vfx = battle.vfx || (battle.vfx = {})"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("lastDrawCallCount"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("lastInstanceCount"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("victoryContinueEnabled"), std::string::npos);

    EXPECT_NE(sprite_batch_header.find("RedAsAlpha"), std::string::npos);
    EXPECT_NE(texture_shader.find("uTextureMode"), std::string::npos);
    EXPECT_NE(texture_shader.find("texColor = vec4(1.0, 1.0, 1.0, texColor.r);"), std::string::npos);
    EXPECT_NE(text_renderer.find("drawAlphaTexture"), std::string::npos);
    EXPECT_NE(font_manager.find("#if !defined(__EMSCRIPTEN__)"), std::string::npos);

    EXPECT_NE(web_smoke.find("FULL_RPG_RUNTIME_PACKAGE_IDS"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_battle_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("trigger_town_encounter_from_diagnostics"), std::string::npos);
    EXPECT_NE(web_smoke.find("preferred_troop_id"), std::string::npos);
    EXPECT_NE(web_smoke.find("screenshot_prefix"), std::string::npos);
    EXPECT_NE(web_smoke.find("home_exterior_to_town"), std::string::npos);
    EXPECT_NE(web_smoke.find("town_enemy_encounter"), std::string::npos);
    EXPECT_NE(web_smoke.find("battle_skill_vfx"), std::string::npos);
    EXPECT_NE(web_smoke.find("battle_victory_return_to_map"), std::string::npos);
    EXPECT_NE(web_smoke.find("battle_reward_writeback"), std::string::npos);
    EXPECT_NE(web_smoke.find("GameScene: Battle ended, outcome=Victory"), std::string::npos);
    EXPECT_NE(web_smoke.find("victoryContinueEnabled"), std::string::npos);
    EXPECT_NE(web_smoke.find("expected_diagnostic_map = str(current_gameplay.get(\"map\") or \"home_exterior\")"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/farm-rpg/UI/Clock/Clock.png"), std::string::npos);
    EXPECT_NE(boot_manifest.find("assets/farm-rpg/UI/Clock/clock hand.png"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase26FullRpgBasicGameplayFlowsArePresent) {
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string shop_scene_source = readProjectFile("src/game/scene/shop_menu_scene.cpp");
    const std::string quest_offer_source = readProjectFile("src/game/scene/quest_offer_scene.cpp");
    const std::string recruit_offer_source = readProjectFile("src/game/scene/recruit_offer_scene.cpp");
    const std::string rest_scene_source = readProjectFile("src/game/scene/rest_dialog_scene.cpp");
    const std::string appearance_scene_source = readProjectFile("src/game/scene/appearance_customize_scene.cpp");
    const std::string quest_system_source = readProjectFile("src/game/system/quest_interaction_system.cpp");
    const std::string recruitment_system_source = readProjectFile("src/game/system/party_recruitment_system.cpp");
    const std::string rest_system_source = readProjectFile("src/game/system/rest_system.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");

    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(shop_scene_source.empty());
    ASSERT_FALSE(quest_offer_source.empty());
    ASSERT_FALSE(recruit_offer_source.empty());
    ASSERT_FALSE(rest_scene_source.empty());
    ASSERT_FALSE(appearance_scene_source.empty());
    ASSERT_FALSE(quest_system_source.empty());
    ASSERT_FALSE(recruitment_system_source.empty());
    ASSERT_FALSE(rest_system_source.empty());
    ASSERT_FALSE(web_smoke.empty());

    EXPECT_NE(game_scene_source.find("inventory.items = parseCounts"), std::string::npos);
    EXPECT_NE(game_scene_source.find("party.activeActorIds"), std::string::npos);
    EXPECT_NE(game_scene_source.find("party.runtimeStates"), std::string::npos);
    EXPECT_NE(game_scene_source.find("quests.objectiveProgress"), std::string::npos);
    EXPECT_NE(game_scene_source.find("appearance.signature"), std::string::npos);
    EXPECT_NE(game_scene_source.find("time.day"), std::string::npos);

    EXPECT_NE(shop_scene_source.find("ShopMenuScene: opened"), std::string::npos);
    EXPECT_NE(shop_scene_source.find("ShopMenuScene: buy completed"), std::string::npos);
    EXPECT_NE(shop_scene_source.find("ShopMenuScene: sell completed"), std::string::npos);
    EXPECT_NE(shop_scene_source.find("ShopMenuScene: buy failed"), std::string::npos);
    EXPECT_NE(quest_offer_source.find("onMenuConfirmPressed"), std::string::npos);
    EXPECT_NE(recruit_offer_source.find("onMenuConfirmPressed"), std::string::npos);
    EXPECT_NE(rest_scene_source.find("RestDialogScene: opened."), std::string::npos);
    EXPECT_NE(rest_scene_source.find("RestDialogScene: confirmed hours="), std::string::npos);
    EXPECT_NE(appearance_scene_source.find("AppearanceCustomizeScene: opened mode=closet."), std::string::npos);
    EXPECT_NE(appearance_scene_source.find("AppearanceCustomizeScene: confirmed closet appearance."), std::string::npos);
    EXPECT_NE(quest_system_source.find("QuestInteractionSystem: quest accepted"), std::string::npos);
    EXPECT_NE(quest_system_source.find("QuestInteractionSystem: quest completed"), std::string::npos);
    EXPECT_NE(recruitment_system_source.find("PartyRecruitmentSystem: recruited actor_id="), std::string::npos);
    EXPECT_NE(rest_system_source.find("RestSystem: rest confirmed hours="), std::string::npos);

    EXPECT_NE(web_smoke.find("exercise_full_rpg_basic_flows"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_shop_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_quest_accept_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_recruit_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_quest_battle_and_turn_in_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_rest_and_wardrobe_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("exercise_full_rpg_save_reload_verify"), std::string::npos);
    EXPECT_NE(web_smoke.find("quest.village.goblin_cleanup::kill_slimes"), std::string::npos);
    EXPECT_NE(web_smoke.find("required_slime_kills = 3"), std::string::npos);
    EXPECT_NE(web_smoke.find("preferred_troop_id = \"troop.slime\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"troop.slime_single\""), std::string::npos);
    EXPECT_NE(web_smoke.find("phase26-quest-battle-"), std::string::npos);
    EXPECT_NE(web_smoke.find("shop_buy_sell_failure_feedback"), std::string::npos);
    EXPECT_NE(web_smoke.find("quest_accept_progress_turn_in_reward"), std::string::npos);
    EXPECT_NE(web_smoke.find("recruit_accept_party_writeback"), std::string::npos);
    EXPECT_NE(web_smoke.find("rest_recovery_time_advance"), std::string::npos);
    EXPECT_NE(web_smoke.find("wardrobe_appearance_change"), std::string::npos);
    EXPECT_NE(web_smoke.find("full_rpg_save_reload_verify"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase27HdrBloomRuntimeGateIsPresent) {
    const std::string renderer_header = readProjectFile("src/engine/render/opengl/gl_renderer.h");
    const std::string renderer_source = readProjectFile("src/engine/render/opengl/gl_renderer.cpp");
    const std::string bloom_pass_source = readProjectFile("src/engine/render/opengl/bloom_pass.cpp");
    const std::string emissive_pass_source = readProjectFile("src/engine/render/opengl/emissive_pass.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");

    ASSERT_FALSE(renderer_header.empty());
    ASSERT_FALSE(renderer_source.empty());
    ASSERT_FALSE(bloom_pass_source.empty());
    ASSERT_FALSE(emissive_pass_source.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_validator_source.empty());

    EXPECT_NE(renderer_header.find("rgba16f_color_renderable"), std::string::npos);
    EXPECT_NE(renderer_header.find("hdr_fallback_reason"), std::string::npos);
    EXPECT_NE(renderer_header.find("emissive_draw_calls"), std::string::npos);
    EXPECT_NE(renderer_header.find("bloom_levels"), std::string::npos);

    EXPECT_NE(renderer_source.find("detectRenderCapabilities"), std::string::npos);
    EXPECT_NE(renderer_source.find("queryWebGlExtension(\"EXT_color_buffer_float\")"), std::string::npos);
    EXPECT_NE(renderer_source.find("queryWebGlExtension(\"OES_texture_float_linear\")"), std::string::npos);
    EXPECT_NE(renderer_source.find("probeColorRenderableFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT)"),
              std::string::npos);
    EXPECT_NE(renderer_source.find("HDR post-processing fallback: reason="), std::string::npos);
    EXPECT_EQ(renderer_source.find("kEnableHdrPostProcessingByDefault"), std::string::npos);

    EXPECT_NE(bloom_pass_source.find("GL_RGBA16F"), std::string::npos);
    EXPECT_NE(bloom_pass_source.find("GL_HALF_FLOAT"), std::string::npos);
    EXPECT_EQ(bloom_pass_source.find("GL_RGB16F"), std::string::npos);
    EXPECT_NE(emissive_pass_source.find("desc.type = GL_HALF_FLOAT"), std::string::npos);

    EXPECT_NE(web_smoke.find("rgba16fColorRenderable"), std::string::npos);
    EXPECT_NE(web_smoke.find("wait_for_render_postprocessing_activity"), std::string::npos);
    EXPECT_NE(web_smoke.find("hdr_bloom_postprocessing_smoke"), std::string::npos);
    EXPECT_NE(release_validator_source.find("probeColorRenderableFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT)"),
              std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase19PersistentSettingsAndStorageHardeningIsPresent) {
    const std::string engine_events = readProjectFile("src/engine/utils/events.h");
    const std::string game_app_source = readProjectFile("src/engine/core/game_app.cpp");
    const std::string persistent_storage_source = readProjectFile("src/engine/platform/web_persistent_storage.cpp");
    const std::string save_service_header = readProjectFile("src/game/save/save_service.h");
    const std::string save_service_source = readProjectFile("src/game/save/save_service.cpp");
    const std::string user_settings_source = readProjectFile("src/game/runtime/user_settings_service.cpp");
    const std::string title_scene_source = readProjectFile("src/game/scene/title_scene.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string pause_menu_source = readProjectFile("src/game/scene/pause_menu_scene.cpp");
    const std::string save_slot_source = readProjectFile("src/game/scene/save_slot_select_scene.cpp");
    const std::string pause_menu_rml = readProjectFile("ui/rmlui/scenes/pause_menu.rml");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string en_us = readProjectFile("assets/i18n/en-US.json");
    const std::string zh_hans = readProjectFile("assets/i18n/zh-Hans.json");

    ASSERT_FALSE(engine_events.empty());
    ASSERT_FALSE(game_app_source.empty());
    ASSERT_FALSE(persistent_storage_source.empty());
    ASSERT_FALSE(save_service_header.empty());
    ASSERT_FALSE(save_service_source.empty());
    ASSERT_FALSE(user_settings_source.empty());
    ASSERT_FALSE(title_scene_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(pause_menu_source.empty());
    ASSERT_FALSE(save_slot_source.empty());
    ASSERT_FALSE(pause_menu_rml.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(en_us.empty());
    ASSERT_FALSE(zh_hans.empty());

    EXPECT_NE(persistent_storage_source.find("TinyFarmRPGWebReleaseDiagnostics"), std::string::npos);
    EXPECT_NE(persistent_storage_source.find("persistentStorage"), std::string::npos);
    EXPECT_NE(persistent_storage_source.find("from_browser"), std::string::npos);
    EXPECT_NE(persistent_storage_source.find("to_browser"), std::string::npos);
    EXPECT_NE(engine_events.find("WebPersistentStorageReadyEvent"), std::string::npos);
    EXPECT_NE(game_app_source.find("trigger(utils::WebPersistentStorageReadyEvent"), std::string::npos);
    EXPECT_NE(title_scene_source.find("onWebPersistentStorageReady"), std::string::npos);
    EXPECT_NE(title_scene_source.find("Web persistent settings reloaded after storage sync"), std::string::npos);
    EXPECT_NE(game_scene_source.find("onWebPersistentStorageReady"), std::string::npos);

    EXPECT_EQ(save_service_header.find("deleteSlot"), std::string::npos);
    EXPECT_EQ(save_service_source.find("after slot delete"), std::string::npos);
    EXPECT_NE(user_settings_source.find("UserSettingsService: Web persistent settings sync"), std::string::npos);
    EXPECT_NE(user_settings_source.find("userSettings"), std::string::npos);

    EXPECT_EQ(pause_menu_source.find("SaveSlotSelectScene::Mode::Delete"), std::string::npos);
    EXPECT_EQ(pause_menu_source.find("game::save::SaveService::deleteSlot"), std::string::npos);
    EXPECT_EQ(save_slot_source.find("Mode::Delete"), std::string::npos);
    EXPECT_EQ(save_slot_source.find("\"save_slot.confirm.delete\""), std::string::npos);
    EXPECT_EQ(pause_menu_rml.find("data-event-click=\"delete_save\""), std::string::npos);
    EXPECT_EQ(pause_menu_rml.find("\"common.delete\""), std::string::npos);

    EXPECT_NE(web_smoke.find("exercise_settings_persistence"), std::string::npos);
    EXPECT_NE(web_smoke.find("verify_user_settings_restored"), std::string::npos);
    EXPECT_NE(web_smoke.find("write_corrupt_save_slot"), std::string::npos);
    EXPECT_NE(web_smoke.find("persistent_storage_logs"), std::string::npos);
    EXPECT_EQ(web_smoke.find("delete_slot0_via_pause_menu"), std::string::npos);
    EXPECT_EQ(web_smoke.find("delete_slot_sync_reload_absent"), std::string::npos);
    EXPECT_EQ(web_smoke.find("slot_delete_sync_completed"), std::string::npos);

    EXPECT_EQ(en_us.find("\"common.delete\""), std::string::npos);
    EXPECT_EQ(en_us.find("\"save_slot.confirm.delete\""), std::string::npos);
    EXPECT_EQ(zh_hans.find("\"common.delete\""), std::string::npos);
    EXPECT_EQ(zh_hans.find("\"save_slot.confirm.delete\""), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase20ReleaseRunbookArtifactsDocsAndCiArePresent) {
    const std::string runbook = readProjectFile("tools/web_release/web_release_runbook.py");
    const std::string release_docs = readProjectFile("docs/web_release.md");
    const std::string docs_index = readProjectFile("docs/README.md");
    const std::string workflow = readProjectFile(".github/workflows/web-release.yml");

    ASSERT_FALSE(runbook.empty());
    ASSERT_FALSE(release_docs.empty());
    ASSERT_FALSE(docs_index.empty());
    ASSERT_FALSE(workflow.empty());

    EXPECT_NE(runbook.find("artifact-manifest.json"), std::string::npos);
    EXPECT_NE(runbook.find("release-report.md"), std::string::npos);
    EXPECT_NE(runbook.find("sha256_file"), std::string::npos);
    EXPECT_NE(runbook.find("gzip_file_bytes"), std::string::npos);
    EXPECT_NE(runbook.find("brotli_file_bytes"), std::string::npos);
    EXPECT_NE(runbook.find("application/wasm"), std::string::npos);
    EXPECT_NE(runbook.find("application/octet-stream"), std::string::npos);
    EXPECT_NE(runbook.find("not required for the default single-thread build"), std::string::npos);
    EXPECT_NE(runbook.find("write_release_markdown"), std::string::npos);

    EXPECT_NE(release_docs.find("python3 tools/web_release/web_release_runbook.py auto"), std::string::npos);
    EXPECT_NE(release_docs.find("artifact-manifest.json"), std::string::npos);
    EXPECT_NE(release_docs.find("release-report.md"), std::string::npos);
    EXPECT_NE(release_docs.find("application/wasm"), std::string::npos);
    EXPECT_NE(release_docs.find("application/octet-stream"), std::string::npos);
    EXPECT_NE(release_docs.find("Clear site data"), std::string::npos);
    EXPECT_NE(release_docs.find("默认单线程发布不需要"), std::string::npos);
    EXPECT_NE(docs_index.find("web_release.md"), std::string::npos);

    EXPECT_NE(workflow.find("web-release-gate"), std::string::npos);
    EXPECT_NE(workflow.find("setup-emsdk@v14"), std::string::npos);
    EXPECT_NE(workflow.find("version: 5.0.7"), std::string::npos);
    EXPECT_NE(workflow.find("--skip-smoke"), std::string::npos);
    EXPECT_NE(workflow.find("artifact-manifest.json"), std::string::npos);
    EXPECT_NE(workflow.find("release-report.md"), std::string::npos);
    EXPECT_NE(workflow.find("chromium-smoke"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase28FullRpgReleaseDocsRunbookCiAndReportAreCurrent) {
    const std::string runbook = readProjectFile("tools/web_release/web_release_runbook.py");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_docs = readProjectFile("docs/web_release.md");
    const std::string workflow = readProjectFile(".github/workflows/web-release.yml");
    const std::string battle_scene_header = readProjectFile("src/game/scene/battle_scene.h");
    const std::string battle_scene_source = readProjectFile("src/game/scene/battle_scene.cpp");
    const std::string web_shell = readProjectFile("src/web/web_shell_ui.cpp");
    const std::string phase28_report = readProjectFile("plans/reports/2026-06-05-web-release-phase-28-report.md");
    const std::string final_report = readProjectFile("plans/reports/2026-06-05-web-release-final-full-rpg-report.md");

    ASSERT_FALSE(runbook.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_docs.empty());
    ASSERT_FALSE(workflow.empty());
    ASSERT_FALSE(battle_scene_header.empty());
    ASSERT_FALSE(battle_scene_source.empty());
    ASSERT_FALSE(web_shell.empty());
    ASSERT_FALSE(phase28_report.empty());
    ASSERT_FALSE(final_report.empty());

    EXPECT_NE(runbook.find("\"--profile\""), std::string::npos);
    EXPECT_NE(runbook.find("\"--smoke-profile\""), std::string::npos);
    EXPECT_NE(runbook.find("smoke_profile"), std::string::npos);
    EXPECT_NE(runbook.find("Runtime Package Index"), std::string::npos);
    EXPECT_NE(runbook.find("Runtime Package Responses"), std::string::npos);
    EXPECT_NE(runbook.find("Render Capabilities"), std::string::npos);
    EXPECT_NE(runbook.find("Gameplay Coverage"), std::string::npos);
    EXPECT_NE(runbook.find("package_load_events"), std::string::npos);

    EXPECT_NE(release_docs.find("--profile full-rpg"), std::string::npos);
    EXPECT_NE(release_docs.find("Effekseer WebGL2 后端"), std::string::npos);
    EXPECT_NE(release_docs.find("HDR emissive 与 Bloom"), std::string::npos);
    EXPECT_NE(release_docs.find("Attack / Guard / Item / Escape"), std::string::npos);
    EXPECT_NE(release_docs.find("battle-core.tfpack"), std::string::npos);
    EXPECT_NE(release_docs.find("vfx-core.tfpack"), std::string::npos);
    EXPECT_NE(release_docs.find("IDBFS"), std::string::npos);
    EXPECT_EQ(release_docs.find("no-bloom"), std::string::npos);
    EXPECT_EQ(release_docs.find("后续增强项"), std::string::npos);

    EXPECT_NE(workflow.find("smoke_profile"), std::string::npos);
    EXPECT_NE(workflow.find("default: \"full-rpg\""), std::string::npos);
    EXPECT_NE(workflow.find("--profile ${{ inputs.smoke_profile }}"), std::string::npos);
    EXPECT_NE(workflow.find("web-packages/web-package-index.json"), std::string::npos);
    EXPECT_NE(workflow.find("chromium-smoke-failed.json"), std::string::npos);

    EXPECT_NE(battle_scene_header.find("last_action_sequence_"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("lastAction.sequence"), std::string::npos);
    EXPECT_NE(battle_scene_source.find("lastActionHistory"), std::string::npos);

    EXPECT_NE(web_smoke.find("exercise_full_rpg_battle_depth_flow"), std::string::npos);
    EXPECT_NE(web_smoke.find("matching_battle_action"), std::string::npos);
    EXPECT_NE(web_smoke.find("submit_battle_attack_action"), std::string::npos);
    EXPECT_NE(web_smoke.find("submit_battle_guard_action"), std::string::npos);
    EXPECT_NE(web_smoke.find("submit_battle_item_action"), std::string::npos);
    EXPECT_NE(web_smoke.find("run_battle_escape_until_finished"), std::string::npos);
    EXPECT_NE(web_smoke.find("run_battle_to_defeat"), std::string::npos);
    EXPECT_NE(web_smoke.find("party_runtime_states"), std::string::npos);
    EXPECT_NE(web_smoke.find("defeated_encounters_from_save"), std::string::npos);
    EXPECT_NE(web_smoke.find("\"battle_attack_item_guard_escape_matrix\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"battle_defeat_flow\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"battle_hp_mp_inventory_writeback\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"battle_defeated_encounter_save_reload_matrix\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"pending_flows\": []"), std::string::npos);

    EXPECT_NE(web_shell.find("Full RPG profile reports RmlUi, Effekseer, and Bloom through runtime diagnostics."),
              std::string::npos);
    EXPECT_EQ(web_shell.find("remain deferred"), std::string::npos);

    EXPECT_NE(phase28_report.find("web_release_runbook.py auto --skip-build --profile full-rpg"), std::string::npos);
    EXPECT_NE(final_report.find("Effekseer"), std::string::npos);
    EXPECT_NE(final_report.find("Bloom"), std::string::npos);
    EXPECT_NE(final_report.find("battle_attack_item_guard_escape_matrix"), std::string::npos);
    EXPECT_NE(final_report.find("battle_defeat_flow"), std::string::npos);
    EXPECT_NE(final_report.find("battle_hp_mp_inventory_writeback"), std::string::npos);
    EXPECT_NE(final_report.find("battle_defeated_encounter_save_reload_matrix"), std::string::npos);
    EXPECT_EQ(final_report.find("Uncovered non-basic flows"), std::string::npos);
    EXPECT_EQ(final_report.find("基础 RPG 玩法列为后续增强"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase22FullRpgResourceTopologyIsPresent) {
    const std::string asset_audit_source = readProjectFile("tools/asset_audit/audit_assets.py");
    const std::string package_tool = readProjectFile("tools/web_release/package_web_assets.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");
    const std::string package_registry_header = readProjectFile("src/engine/platform/web_asset_package_registry.h");
    const std::string package_registry_source = readProjectFile("src/engine/platform/web_asset_package_registry.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_manager_source = readProjectFile("src/game/world/map_manager.cpp");
    const std::string release_manifest = readProjectFile("manifests/assets/web-release-full.args");
    const std::string release_asset_list = readProjectFile("manifests/assets/web-release-full-assets.txt");
    const std::string boot_manifest = readProjectFile("manifests/assets/web-release-boot.args");

    ASSERT_FALSE(asset_audit_source.empty());
    ASSERT_FALSE(package_tool.empty());
    ASSERT_FALSE(release_validator_source.empty());
    ASSERT_FALSE(package_registry_header.empty());
    ASSERT_FALSE(package_registry_source.empty());
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(map_manager_source.empty());
    ASSERT_FALSE(release_manifest.empty());
    ASSERT_FALSE(release_asset_list.empty());
    ASSERT_FALSE(boot_manifest.empty());

    EXPECT_NE(asset_audit_source.find("select_web_full_rpg_assets"), std::string::npos);
    EXPECT_NE(asset_audit_source.find("web-release-full-assets.txt"), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"assets/maps/town.tmj\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/dialogue_choice.\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"assets/maps/school.tmj\""), std::string::npos);

    EXPECT_NE(release_manifest.find("assets/maps/town.tmj"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/battle.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/dialogue_choice.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/quest_offer.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/recruit_offer.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/rest_dialog.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("ui/rmlui/scenes/shop_menu.rml"), std::string::npos);
    EXPECT_NE(release_manifest.find("assets/farm-rpg/Character and Portrait/Character/PNG/1. Idle/Clothers/Farm/Purple.png"),
              std::string::npos);
    EXPECT_NE(release_manifest.find("assets/farm-rpg/Character and Portrait/Character/PNG/1. Idle/Hair's/Fawn/Black.png"),
              std::string::npos);
    EXPECT_NE(release_manifest.find("assets/farm-rpg/Character and Portrait/Character/PNG/1. Idle/Acc/Wizard.png"),
              std::string::npos);
    EXPECT_NE(release_manifest.find("assets/farm-rpg/Character and Portrait/Portrait/PNG/Hair/Fawn/Black.png"),
              std::string::npos);
    EXPECT_NE(release_manifest.find("assets/farm-rpg/Character and Portrait/Portrait/PNG/Acc/Wizard.png"),
              std::string::npos);
    EXPECT_NE(release_manifest.find("assets/textures/BattleBg/battlebacks1/Grassland.png"), std::string::npos);
    EXPECT_NE(release_manifest.find("assets/vfx/effects/HitEffect.efkefc"), std::string::npos);
    EXPECT_EQ(release_manifest.find("assets/maps/school.tmj"), std::string::npos);

    EXPECT_NE(package_tool.find("\"rpg-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"town-map\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"battle-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"vfx-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("PACKAGE_DEPENDENCIES"), std::string::npos);
    EXPECT_NE(package_tool.find("assets/farm-rpg/UI/Clock/Clock.png"), std::string::npos);
    EXPECT_NE(package_tool.find("assets/farm-rpg/Enemy/"), std::string::npos);
    EXPECT_NE(release_validator_source.find("assets/farm-rpg/Enemy/Slimes/Blue/Slime/Idle.png"), std::string::npos);
    EXPECT_NE(package_tool.find("assets/vfx/"), std::string::npos);
    EXPECT_NE(package_tool.find("assets/textures/BattleBg/"), std::string::npos);

    EXPECT_NE(release_validator_source.find("REQUIRED_PACKAGE_DEPENDENCIES"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_TOWN_MAP_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_BATTLE_CORE_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_VFX_CORE_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("FORBIDDEN_FULL_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("must record a positive byte count"), std::string::npos);

    EXPECT_NE(package_registry_header.find("PACKAGE_TOWN_MAP"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_BATTLE_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("PACKAGE_VFX_CORE"), std::string::npos);
    EXPECT_NE(package_registry_header.find("loadGroup"), std::string::npos);
    EXPECT_NE(package_registry_header.find("packageFiles"), std::string::npos);
    EXPECT_NE(package_registry_header.find("packageBytes"), std::string::npos);
    EXPECT_EQ(package_registry_source.find("std::array<PackageDefinition, 3>"), std::string::npos);
    EXPECT_NE(game_scene_source.find("PACKAGE_RPG_CORE"), std::string::npos);
    EXPECT_NE(map_manager_source.find("map_name == \"town\""), std::string::npos);
    EXPECT_NE(map_manager_source.find("PACKAGE_TOWN_MAP"), std::string::npos);

    EXPECT_EQ(boot_manifest.find("assets/maps/town.tmj"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("assets/vfx/effects/HitEffect.efkefc"), std::string::npos);
    EXPECT_EQ(boot_manifest.find("ui/rmlui/scenes/battle.rml"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase23WebDiagnosticsAndSmokeProfilesArePresent) {
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string package_registry_source = readProjectFile("src/engine/platform/web_asset_package_registry.cpp");
    const std::string runtime_service_factory = readProjectFile("src/game/runtime/runtime_service_factory.cpp");
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string runbook = readProjectFile("tools/web_release/web_release_runbook.py");

    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(package_registry_source.empty());
    ASSERT_FALSE(runtime_service_factory.empty());
    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(runbook.empty());

    EXPECT_NE(game_scene_source.find("TinyFarmRPGWebReleaseDiagnostics"), std::string::npos);
    EXPECT_NE(game_scene_source.find("diagnostics.gameplay"), std::string::npos);
    EXPECT_NE(game_scene_source.find("currentScene = \"GameScene\""), std::string::npos);
    EXPECT_NE(game_scene_source.find("occupiedSlots"), std::string::npos);
    EXPECT_NE(game_scene_source.find("activeQuestIds"), std::string::npos);
    EXPECT_NE(game_scene_source.find("recruitedCount"), std::string::npos);

    EXPECT_NE(package_registry_source.find("diagnostics.packages"), std::string::npos);
    EXPECT_NE(package_registry_source.find("entry.dependencies = dependencyText.length"), std::string::npos);
    EXPECT_NE(package_registry_source.find("lastLoadMs"), std::string::npos);
    EXPECT_NE(package_registry_source.find("lastError"), std::string::npos);

    EXPECT_NE(runtime_service_factory.find("publishWebVfxDiagnostics"), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("diagnostics.vfx"), std::string::npos);
    EXPECT_NE(runtime_service_factory.find("effekseerEnabled"), std::string::npos);

    EXPECT_NE(web_smoke.find("\"--profile\""), std::string::npos);
    EXPECT_NE(web_smoke.find("\"full-rpg\""), std::string::npos);
    EXPECT_NE(web_smoke.find("read_web_release_diagnostics"), std::string::npos);
    EXPECT_NE(web_smoke.find("validate_web_release_diagnostics"), std::string::npos);
    EXPECT_NE(web_smoke.find("full_rpg_profile_diagnostics_gate"), std::string::npos);
    EXPECT_NE(web_smoke.find("web_release_diagnostics_snapshot"), std::string::npos);

    EXPECT_NE(runbook.find("--smoke-profile"), std::string::npos);
    EXPECT_NE(runbook.find("args.smoke_profile"), std::string::npos);
    EXPECT_NE(runbook.find("\"--profile\""), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, BlueprintManagerAvoidsJsonExceptionPaths) {
    const std::string blueprint_source = readProjectFile("src/game/factory/blueprint_manager.cpp");

    ASSERT_FALSE(blueprint_source.empty());

    EXPECT_NE(blueprint_source.find("#include \"engine/utils/json_helpers.h\""), std::string::npos);
    EXPECT_NE(blueprint_source.find("nestedNumberOr"), std::string::npos);
    EXPECT_EQ(blueprint_source.find("_json_pointer"), std::string::npos);
    EXPECT_EQ(blueprint_source.find(".value(\""), std::string::npos);
    EXPECT_EQ(blueprint_source.find(".get<"), std::string::npos);
}

} // namespace
// NOLINTEND
