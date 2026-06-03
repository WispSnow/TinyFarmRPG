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
    const std::string renderer_source = readProjectFile("src/engine/render/opengl/gl_renderer.cpp");

    ASSERT_FALSE(platform_header.empty());
    ASSERT_FALSE(renderer_source.empty());

    EXPECT_NE(platform_header.find("TF_GL_PLATFORM_WEBGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsDefaultFramebufferSrgb = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsFloatColorFramebuffers = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kEnableHdrPostProcessingByDefault = kSupportsFloatColorFramebuffers"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepthf(depth);"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepth(static_cast<GLdouble>(depth));"), std::string::npos);
    EXPECT_NE(renderer_source.find("engine::platform::gl::kSupportsDefaultFramebufferSrgb"), std::string::npos);
    EXPECT_NE(renderer_source.find("if constexpr (engine::platform::gl::kEnableHdrPostProcessingByDefault)"),
              std::string::npos);
    EXPECT_NE(renderer_source.find("bloom_enabled_ && bloom_pass_ && emissive_pass_"), std::string::npos);
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
    const std::string preload_manifest = readProjectFile("manifests/assets/web-poc-preload.args");

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
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/pause_menu.\""), std::string::npos);
    EXPECT_NE(asset_audit_source.find("\"ui/rmlui/scenes/save_slot_select.\""), std::string::npos);

    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/appearance_customize.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/pause_menu.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"ui/rmlui/scenes/save_slot_select.rml\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("build_dir / \"TinyFarmRPG-Web-preload-root\""), std::string::npos);

    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/title.rml"), std::string::npos);
    EXPECT_NE(preload_manifest.find("ui/rmlui/scenes/appearance_customize.rml"), std::string::npos);
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
}

TEST(WebGameplayTargetSourceTest, Phase12AsyncSaveCompletionWaitsForPersistentStorageSync) {
    const std::string save_service_source = readProjectFile("src/game/save/save_service.cpp");
    const std::string persistent_storage_source = readProjectFile("src/engine/platform/web_persistent_storage.cpp");

    ASSERT_FALSE(save_service_source.empty());
    ASSERT_FALSE(persistent_storage_source.empty());

    EXPECT_NE(save_service_source.find("#include \"engine/platform/web_persistent_storage.h\""), std::string::npos);
    EXPECT_NE(save_service_source.find("syncPersistentStorageToBrowser(&onAsyncSavePersistentSync"), std::string::npos);
    EXPECT_NE(save_service_source.find("appendPersistentSyncError"), std::string::npos);
    EXPECT_NE(save_service_source.find("completion->save_in_progress->store(false"), std::string::npos);
    EXPECT_NE(save_service_source.find("enqueueAsyncSaveCompleted(*completion->main_thread_queue"), std::string::npos);
    EXPECT_NE(save_service_source.find("syncPersistentStorageToBrowser(&onDirectSavePersistentSync"), std::string::npos);

    EXPECT_NE(persistent_storage_source.find("bool in_progress{false}"), std::string::npos);
    EXPECT_NE(persistent_storage_source.find("TinyFarmRPG persistent FS sync is already in progress."), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase12PreloadsMinimalAudioLoopResources) {
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");
    const std::string preload_manifest = readProjectFile("manifests/assets/web-poc-preload.args");

    ASSERT_FALSE(release_validator_source.empty());
    ASSERT_FALSE(preload_manifest.empty());

    EXPECT_NE(release_validator_source.find("\"assets/audio/pop.mp3\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/01_spring_journey.ogg\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"assets/audio/02_spring_fairy_tale.ogg\""), std::string::npos);
    EXPECT_NE(release_validator_source.find("\"config/audio.json\""), std::string::npos);

    EXPECT_NE(preload_manifest.find("assets/audio/pop.mp3"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/01_spring_journey.ogg"), std::string::npos);
    EXPECT_NE(preload_manifest.find("assets/audio/02_spring_fairy_tale.ogg"), std::string::npos);
    EXPECT_NE(preload_manifest.find("config/audio.json"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase13RuntimePackagePipelineIsPresent) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");
    const std::string src_cmake = readProjectFile("src/CMakeLists.txt");
    const std::string package_loader_header = readProjectFile("src/engine/platform/web_asset_package.h");
    const std::string package_loader_source = readProjectFile("src/engine/platform/web_asset_package.cpp");
    const std::string game_scene_source = readProjectFile("src/game/scene/game_scene.cpp");
    const std::string map_manager_source = readProjectFile("src/game/world/map_manager.cpp");
    const std::string package_tool = readProjectFile("tools/web_release/package_web_assets.py");
    const std::string release_validator_source = readProjectFile("tools/web_release/validate_web_release.py");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(runtime_cmake.empty());
    ASSERT_FALSE(src_cmake.empty());
    ASSERT_FALSE(package_loader_header.empty());
    ASSERT_FALSE(package_loader_source.empty());
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
    EXPECT_NE(package_loader_header.find("loadAssetPackage"), std::string::npos);
    EXPECT_NE(package_loader_source.find("XMLHttpRequest"), std::string::npos);
    EXPECT_NE(package_loader_source.find("overrideMimeType"), std::string::npos);
    EXPECT_EQ(package_loader_source.find("emscripten_fetch"), std::string::npos);
    EXPECT_NE(package_loader_source.find("PACKAGE_MAGIC"), std::string::npos);
    EXPECT_NE(package_loader_source.find("'T', 'F', 'P', 'K'"), std::string::npos);
    EXPECT_NE(package_loader_source.find("std::filesystem::create_directories"), std::string::npos);

    EXPECT_NE(game_scene_source.find("ensureWebGameplayPackages"), std::string::npos);
    EXPECT_NE(game_scene_source.find("web-packages/home-map.tfpack"), std::string::npos);
    EXPECT_NE(map_manager_source.find("ensureWebMapPackage"), std::string::npos);

    EXPECT_NE(package_tool.find("custom_sync_xhr_fs_writefile"), std::string::npos);
    EXPECT_NE(package_tool.find("\"boot\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"shared-ui\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"home-map\""), std::string::npos);
    EXPECT_NE(package_tool.find("\"audio-core\""), std::string::npos);
    EXPECT_NE(package_tool.find("write_tfpack"), std::string::npos);

    EXPECT_NE(release_validator_source.find("validate_runtime_packages"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_HOME_MAP_PACKAGE_PATHS"), std::string::npos);
    EXPECT_NE(release_validator_source.find("REQUIRED_AUDIO_CORE_PACKAGE_PATHS"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, Phase14ChromiumSmokePipelineIsPresent) {
    const std::string web_smoke = readProjectFile("tools/web_release/web_smoke.py");
    const std::string release_server = readProjectFile("tools/web_release/serve_web_release.py");

    ASSERT_FALSE(web_smoke.empty());
    ASSERT_FALSE(release_server.empty());

    EXPECT_NE(web_smoke.find("configure_web_build"), std::string::npos);
    EXPECT_NE(web_smoke.find("validate_web_release.py"), std::string::npos);
    EXPECT_NE(web_smoke.find("remote-debugging-port"), std::string::npos);
    EXPECT_NE(web_smoke.find("Runtime.consoleAPICalled"), std::string::npos);
    EXPECT_NE(web_smoke.find("Network.responseReceived"), std::string::npos);
    EXPECT_NE(web_smoke.find("click_logical"), std::string::npos);
    EXPECT_NE(web_smoke.find("SAVE_PATH = \"/persistent/saves/slot0.json\""), std::string::npos);
    EXPECT_NE(web_smoke.find("Player did not move far enough"), std::string::npos);
    EXPECT_NE(web_smoke.find("SaveService: 已载入存档 'home_exterior'"), std::string::npos);
    EXPECT_NE(web_smoke.find("home-map.tfpack"), std::string::npos);
    EXPECT_NE(web_smoke.find("Single-thread preview must not require COOP/COEP headers"), std::string::npos);

    EXPECT_NE(release_server.find("\".wasm\": \"application/wasm\""), std::string::npos);
    EXPECT_NE(release_server.find("\".tfpack\": \"application/octet-stream\""), std::string::npos);
    EXPECT_NE(release_server.find("Cache-Control"), std::string::npos);
    EXPECT_NE(release_server.find("Cross-Origin-Opener-Policy"), std::string::npos);
    EXPECT_NE(release_server.find("Cross-Origin-Embedder-Policy"), std::string::npos);
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
