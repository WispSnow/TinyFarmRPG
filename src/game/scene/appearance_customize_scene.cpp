#include "game/scene/appearance_customize_scene.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/options_events.h"
#include "game/runtime/service_lookup.h"
#include "game/system/appearance_layer_cache_builder.h"
#include "game/ui/localized_text.h"
#include "game/ui/player_display_name.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

using namespace entt::literals;

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/appearance_customize.rml";
constexpr std::string_view MODEL_NAME = "appearance_customize";
constexpr std::string_view MENU_PANEL_TEXTURE_PATH = "assets/farm-rpg/UI/Inventory/inventory.png";
constexpr entt::id_type MENU_PANEL_TEXTURE_ID = "ui/appearance/menu-panel"_hs;
const engine::utils::Rect MENU_PANEL_SOURCE_RECT{{0.0f, 64.0f}, {48.0f, 48.0f}};
constexpr engine::render::NineSliceMargins MENU_PANEL_MARGINS{
    .left = 10.0f,
    .top = 10.0f,
    .right = 10.0f,
    .bottom = 10.0f,
};
const engine::utils::Rect MENU_PANEL_SCREEN_RECT{{30.0f, 26.0f}, {580.0f, 308.0f}};
// Tuned against the left header area in appearance_customize.rcss. The transform
// position is the sprite pivot; keep the idle preview to the right of the title text.
constexpr float PREVIEW_SCREEN_PIVOT_X = 230.0f;
constexpr float PREVIEW_SCREEN_PIVOT_Y = 76.0f;
constexpr float PREVIEW_SIZE_PX = 64.0f;
constexpr float FRAME_SIZE_PX = 32.0f;
constexpr float PREVIEW_IDLE_FRAME_DURATION_MS = 200.0f;
constexpr std::size_t MAX_PLAYER_NAME_CODEPOINTS = 16U;

[[nodiscard]] Rml::String makeRmlString(std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] int singleIntArg(const Rml::VariantList& arguments) {
    return arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1;
}

[[nodiscard]] bool hasPositiveSize(const engine::utils::Rect& rect) {
    return rect.size.x > 0.0f && rect.size.y > 0.0f;
}

[[nodiscard]] std::string trimAsciiWhitespace(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
        --end;
    }
    return std::string{begin, end};
}

[[nodiscard]] bool isUtf8ContinuationByte(const unsigned char value) {
    return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] std::size_t utf8CodepointLength(const unsigned char lead) {
    if ((lead & 0x80U) == 0U) {
        return 1U;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3U;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4U;
    }
    return 1U;
}

[[nodiscard]] std::string limitUtf8Codepoints(std::string_view value, const std::size_t max_codepoints) {
    std::size_t byte_index = 0U;
    std::size_t codepoints = 0U;
    while (byte_index < value.size() && codepoints < max_codepoints) {
        const auto lead = static_cast<unsigned char>(value[byte_index]);
        const std::size_t length = utf8CodepointLength(lead);
        if (byte_index + length > value.size()) {
            break;
        }
        bool valid_sequence = true;
        for (std::size_t i = 1U; i < length; ++i) {
            if (!isUtf8ContinuationByte(static_cast<unsigned char>(value[byte_index + i]))) {
                valid_sequence = false;
                break;
            }
        }
        if (!valid_sequence) {
            ++byte_index;
        } else {
            byte_index += length;
        }
        ++codepoints;
    }
    return std::string{value.substr(0U, byte_index)};
}

[[nodiscard]] std::string normalizePlayerName(std::string_view value, std::string_view default_name) {
    std::string normalized = trimAsciiWhitespace(value);
    if (normalized.empty()) {
        normalized = trimAsciiWhitespace(default_name);
    }
    if (normalized.empty()) {
        normalized = game::ui::defaultPlayerDisplayName(nullptr);
    }
    return limitUtf8Codepoints(normalized, MAX_PLAYER_NAME_CODEPOINTS);
}

[[nodiscard]] std::mt19937 makeAppearanceRandomEngine() {
    std::random_device device;
    const auto timestamp = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::seed_seq seed{
        device(),
        device(),
        device(),
        device(),
        static_cast<std::uint32_t>(timestamp),
        static_cast<std::uint32_t>(timestamp >> 32U),
    };
    return std::mt19937{seed};
}

[[nodiscard]] engine::utils::Rect screenRectToWorldRect(const engine::render::Camera& camera,
                                                        const engine::utils::Rect& screen_rect) {
    const glm::vec2 top_left = camera.screenToWorld(screen_rect.pos);
    const glm::vec2 bottom_right = camera.screenToWorld(screen_rect.pos + screen_rect.size);
    return engine::utils::Rect{top_left, bottom_right - top_left};
}

[[nodiscard]] std::array<engine::utils::Rect, 9> buildNineSliceDestinationRects(
    const engine::utils::Rect& target_rect,
    const engine::render::NineSliceMargins& margins) {
    const float horizontal_scale = margins.left + margins.right > 0.0f
                                       ? std::min(1.0f, target_rect.size.x / (margins.left + margins.right))
                                       : 1.0f;
    const float vertical_scale = margins.top + margins.bottom > 0.0f
                                     ? std::min(1.0f, target_rect.size.y / (margins.top + margins.bottom))
                                     : 1.0f;

    const float left = margins.left * horizontal_scale;
    const float right = margins.right * horizontal_scale;
    const float top = margins.top * vertical_scale;
    const float bottom = margins.bottom * vertical_scale;
    const float center_width = std::max(0.0f, target_rect.size.x - left - right);
    const float center_height = std::max(0.0f, target_rect.size.y - top - bottom);

    const std::array<float, 3> x_positions{
        target_rect.pos.x,
        target_rect.pos.x + left,
        target_rect.pos.x + left + center_width,
    };
    const std::array<float, 3> y_positions{
        target_rect.pos.y,
        target_rect.pos.y + top,
        target_rect.pos.y + top + center_height,
    };
    const std::array<float, 3> widths{left, center_width, right};
    const std::array<float, 3> heights{top, center_height, bottom};

    std::array<engine::utils::Rect, 9> rects{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            rects[row * 3 + column] = engine::utils::Rect{
                {x_positions[column], y_positions[row]},
                {widths[column], heights[row]},
            };
        }
    }
    return rects;
}

void drawNineSliceImageInScreenSpace(engine::render::Renderer& renderer,
                                     const engine::render::Camera& camera,
                                     const engine::render::Image& image,
                                     const engine::utils::Rect& screen_rect) {
    const auto* nine_slice = image.ensureNineSlice();
    if (!nine_slice || !hasPositiveSize(screen_rect)) {
        return;
    }

    const auto destination_rects = buildNineSliceDestinationRects(screen_rect, nine_slice->getMargins());
    const auto& source_rects = nine_slice->getSlices();
    for (std::size_t index = 0; index < source_rects.size(); ++index) {
        if (!hasPositiveSize(source_rects[index]) || !hasPositiveSize(destination_rects[index])) {
            continue;
        }

        const engine::component::Sprite sprite{image.getTextureId(), source_rects[index], image.isFlipped()};
        const engine::utils::Rect world_rect = screenRectToWorldRect(camera, destination_rects[index]);
        renderer.drawSprite(sprite, world_rect.pos, world_rect.size);
    }
}

[[nodiscard]] engine::component::Animation makePreviewAnimation() {
    engine::component::Animation animation{};
    animation.name_ = "idle_down";
    animation.texture_id_ = entt::null;
    animation.pivot_ = glm::vec2{0.5f, 0.75f};
    animation.dst_size_ = glm::vec2{PREVIEW_SIZE_PX, PREVIEW_SIZE_PX};
    for (int frame = 0; frame < 4; ++frame) {
        animation.frames_.emplace_back(
            engine::utils::Rect{
                glm::vec2{static_cast<float>(frame) * FRAME_SIZE_PX, 0.0f},
                glm::vec2{FRAME_SIZE_PX, FRAME_SIZE_PX}},
            PREVIEW_IDLE_FRAME_DURATION_MS);
    }
    return animation;
}

} // namespace

namespace game::scene {

AppearanceCustomizeScene::AppearanceCustomizeScene(std::string_view name,
                                                   engine::core::Context& context,
                                                   SceneFactory on_confirm,
                                                   const game::runtime::LocalizationService* localization)
    : engine::scene::Scene(name, context),
      mode_(Mode::NewGame),
      on_new_game_confirm_(std::move(on_confirm)),
      title_localization_(localization),
      rng_(makeAppearanceRandomEngine()) {
}

AppearanceCustomizeScene::AppearanceCustomizeScene(std::string_view name,
                                                   engine::core::Context& context,
                                                   entt::registry& game_registry,
                                                   entt::entity player,
                                                   std::shared_ptr<game::data::AppearanceCatalog> catalog)
    : engine::scene::Scene(name, context),
      mode_(Mode::Closet),
      game_registry_(&game_registry),
      player_(player),
      catalog_(std::move(catalog)),
      rng_(makeAppearanceRandomEngine()) {
}

AppearanceCustomizeScene::~AppearanceCustomizeScene() {
    restoreSceneLighting();
    disconnectRuntimeListeners();
    shutdownUI();
    releaseMenuPanelImage();
}

bool AppearanceCustomizeScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(mode_ == Mode::Closet ? engine::core::State::Paused : engine::core::State::Title);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!ensureCatalog()) {
        return false;
    }

    show_name_input_ = mode_ == Mode::NewGame;
    runtime_slots_ = appearanceControlSlots(*catalog_, mode_ == Mode::NewGame);
    if (mode_ == Mode::Closet && game_registry_ && game_registry_->valid(player_)) {
        if (const auto* appearance = game_registry_->try_get<game::component::AppearanceComponent>(player_)) {
            original_selection_ = makeSelectionFromComponent(*appearance, *catalog_);
        } else {
            original_selection_ = makeDefaultAppearanceSelection(*catalog_);
        }
    } else {
        original_selection_ = makeDefaultAppearanceSelection(*catalog_);
    }
    draft_selection_ = original_selection_;

    if (!initMenuPanelImage()) {
        return false;
    }
    if (!initPreviewEntity()) {
        releaseMenuPanelImage();
        return false;
    }
    if (!initUI()) {
        releaseMenuPanelImage();
        return false;
    }

    suspendSceneLighting();
    connectRuntimeListeners();
    return Scene::init();
}

void AppearanceCustomizeScene::update(float delta_time) {
    if (!isInitialized()) {
        return;
    }

    preview_animation_system_.update(delta_time);
    Scene::update(delta_time);
}

void AppearanceCustomizeScene::render(float interpolation_alpha) {
    if (!isInitialized()) {
        return;
    }

    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();
    if (mode_ == Mode::NewGame) {
        renderer.beginFrame(camera);
    }

    // Closet mode is drawn over GameScene, which has already begun the frame for the
    // shared renderer. Starting another frame here would clear or reset the world pass.
    drawNineSliceImageInScreenSpace(renderer, camera, menu_panel_image_, MENU_PANEL_SCREEN_RECT);
    updatePreviewPosition();
    preview_render_system_.renderPrepared(preview_registry_, renderer, interpolation_alpha);
}

void AppearanceCustomizeScene::clean() {
    restoreSceneLighting();
    shutdownUI();
    releaseMenuPanelImage();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    preview_registry_ = entt::registry{};
    preview_entity_ = entt::null;
    Scene::clean();
}

engine::scene::SceneUiCoverage AppearanceCustomizeScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
}

bool AppearanceCustomizeScene::ensureCatalog() {
    if (catalog_) {
        return catalog_->defaultProfile() != nullptr;
    }

    // New-game customization runs before GameRuntimeServices exists, so it owns this
    // short-lived catalog instance. Closet mode receives the gameplay catalog instead.
    catalog_ = std::make_shared<game::data::AppearanceCatalog>();
    if (!catalog_->loadFromFile("assets/data/appearance_catalog.json")) {
        spdlog::error("AppearanceCustomizeScene: 加载 appearance_catalog.json 失败。");
        return false;
    }
    if (!catalog_->defaultProfile()) {
        spdlog::error("AppearanceCustomizeScene: AppearanceCatalog 缺少默认 profile。");
        return false;
    }
    return true;
}

bool AppearanceCustomizeScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("AppearanceCustomizeScene: RmlUiRuntime 不可用。");
        return false;
    }

    syncLocalizedText(false);
    syncDefaultPlayerName(false);

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("AppearanceCustomizeScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("AppearanceCustomizeScene: 注册 data model 类型失败。");
        document_controller_.unload();
        return false;
    }

    syncSlotViewModels(false);
    syncPortraitPreview(false);
    if (!constructor.Bind("slots", &slot_view_models_) ||
        !constructor.Bind("title_text", &title_text_) ||
        !constructor.Bind("subtitle_text", &subtitle_text_) ||
        !constructor.Bind("player_name", &player_name_) ||
        !constructor.Bind("show_name_input", &show_name_input_) ||
        !constructor.Bind("portrait_src", &portrait_src_)) {
        spdlog::error("AppearanceCustomizeScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(constructor, "slot_prev", &AppearanceCustomizeScene::onSlotPrevEvent, this) ||
        !document_controller_.bindEvent(constructor, "slot_next", &AppearanceCustomizeScene::onSlotNextEvent, this) ||
        !document_controller_.bindSimpleEvent(constructor, "randomize", [this] { onRandomize(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "reset", [this] { onReset(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "confirm", [this] { onConfirm(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "cancel", [this] { onCancel(); })) {
        spdlog::error("AppearanceCustomizeScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("AppearanceCustomizeScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    focusDefaultAction();
    document_controller_.markAllDirty();
    return true;
}

bool AppearanceCustomizeScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }
    if (!game::ui::registerAppearanceCustomizeDataTypes(constructor)) {
        return false;
    }
    data_types_registered_ = true;
    return true;
}

bool AppearanceCustomizeScene::initMenuPanelImage() {
    const auto texture = context_.getResourceManager().loadTexture(MENU_PANEL_TEXTURE_ID, MENU_PANEL_TEXTURE_PATH);
    if (!texture) {
        spdlog::error("AppearanceCustomizeScene: 加载外观背景面板纹理失败: {}", MENU_PANEL_TEXTURE_PATH);
        return false;
    }

    menu_panel_image_ = engine::render::Image{MENU_PANEL_TEXTURE_PATH, MENU_PANEL_TEXTURE_ID, MENU_PANEL_SOURCE_RECT};
    menu_panel_image_.setNineSliceMargins(MENU_PANEL_MARGINS);
    menu_panel_texture_loaded_ = true;
    return true;
}

bool AppearanceCustomizeScene::initPreviewEntity() {
    preview_registry_ = entt::registry{};
    preview_entity_ = preview_registry_.create();

    preview_registry_.emplace<engine::component::TransformComponent>(preview_entity_, glm::vec2{0.0f, 0.0f});
    preview_registry_.emplace<engine::component::RenderComponent>(preview_entity_, engine::component::RenderComponent::MAIN_LAYER, 0.0f);
    preview_registry_.emplace<engine::component::SpriteComponent>(
        preview_entity_,
        engine::component::Sprite{entt::id_type{}, engine::utils::Rect{0.0f, 0.0f, FRAME_SIZE_PX, FRAME_SIZE_PX}},
        glm::vec2{PREVIEW_SIZE_PX, PREVIEW_SIZE_PX},
        glm::vec2{0.5f, 0.75f});

    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace("idle_down"_hs, makePreviewAnimation());
    preview_registry_.emplace<engine::component::AnimationComponent>(
        preview_entity_,
        std::move(animations),
        "idle_down"_hs);

    game::component::AppearanceComponent appearance{};
    applySelectionToComponent(draft_selection_, appearance);
    preview_registry_.emplace<game::component::AppearanceComponent>(preview_entity_, std::move(appearance));
    preview_registry_.emplace<engine::component::LayeredSpriteComponent>(preview_entity_);
    rebuildPreviewCache();
    return true;
}

void AppearanceCustomizeScene::releaseMenuPanelImage() {
    if (!menu_panel_texture_loaded_) {
        return;
    }
    context_.getResourceManager().unloadTexture(MENU_PANEL_TEXTURE_ID);
    menu_panel_texture_loaded_ = false;
    menu_panel_image_ = engine::render::Image{};
}

void AppearanceCustomizeScene::suspendSceneLighting() {
    if (lighting_suspended_) {
        return;
    }

    auto& renderer = context_.getRenderer();
    previous_lighting_enabled_ = renderer.isLightingEnabled();
    renderer.setLightingEnabled(false);
    lighting_suspended_ = true;
}

void AppearanceCustomizeScene::restoreSceneLighting() {
    if (!lighting_suspended_) {
        return;
    }

    context_.getRenderer().setLightingEnabled(previous_lighting_enabled_);
    lighting_suspended_ = false;
}

void AppearanceCustomizeScene::shutdownUI() {
    portrait_preview_registration_.reset();
    portrait_src_.clear();
    document_controller_.unload();
}

void AppearanceCustomizeScene::connectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&AppearanceCustomizeScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().connect<&AppearanceCustomizeScene::onLanguageChanged>(this);
}

void AppearanceCustomizeScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&AppearanceCustomizeScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().disconnect<&AppearanceCustomizeScene::onLanguageChanged>(this);
}

const game::runtime::LocalizationService* AppearanceCustomizeScene::localization() const {
    return game_registry_ ? game::runtime::findLocalizationService(*game_registry_) : title_localization_;
}

void AppearanceCustomizeScene::syncLocalizedText(bool mark_dirty) {
    const auto* loc = localization();
    const bool new_game = mode_ == Mode::NewGame;
    title_text_ = makeRmlString(game::ui::localizeTextOrFallback(
        loc,
        new_game ? "appearance.title.new_game" : "appearance.title.closet",
        new_game ? "Create Hero" : "Wardrobe"));
    subtitle_text_ = makeRmlString(game::ui::localizeTextOrFallback(
        loc,
        new_game ? "appearance.subtitle.new_game" : "appearance.subtitle.closet",
        new_game ? "Choose a look" : "Change outfit"));

    if (mark_dirty) {
        document_controller_.markDirty("title_text");
        document_controller_.markDirty("subtitle_text");
    }
}

void AppearanceCustomizeScene::syncDefaultPlayerName(bool mark_dirty) {
    if (mode_ != Mode::NewGame) {
        player_name_.clear();
        default_player_name_.clear();
        return;
    }

    const std::string previous_default = default_player_name_;
    const std::string next_default = game::ui::defaultPlayerDisplayName(localization());
    const std::string current_name{player_name_};
    const bool should_follow_default = current_name.empty() || current_name == previous_default;

    default_player_name_ = next_default;
    if (should_follow_default) {
        player_name_ = makeRmlString(next_default);
    }

    if (mark_dirty) {
        document_controller_.markDirty("player_name");
    }
}

void AppearanceCustomizeScene::syncSlotViewModels(bool mark_dirty) {
    if (!catalog_) {
        return;
    }
    slot_view_models_ = game::ui::buildAppearanceSlotViewModels(
        *catalog_,
        draft_selection_,
        localization(),
        mode_ == Mode::NewGame);
    if (mark_dirty) {
        document_controller_.markDirty("slots");
    }
}

void AppearanceCustomizeScene::syncPortraitPreview(bool mark_dirty) {
    auto* runtime = context_.getRmlUi();
    if (!runtime || !catalog_) {
        return;
    }

    portrait_preview_registration_.reset();
    portrait_src_.clear();
    auto images = portrait_builder_.build(*catalog_, draft_selection_);
    if (images.valid()) {
        const std::string source_uri =
            "generated://appearance-preview/" + std::to_string(instanceId()) + "/" + images.selection_key;
        auto registration = runtime->generatedImages().registerImage(
            source_uri,
            std::move(images.standard64),
            engine::ui::rmlui::RmlUiTextureFilterMode::Nearest);
        if (registration.valid()) {
            portrait_src_ = makeRmlString(source_uri);
            portrait_preview_registration_ = std::move(registration);
        }
    }

    if (mark_dirty) {
        document_controller_.markDirty("portrait_src");
    }
}

void AppearanceCustomizeScene::rebuildPreviewCache() {
    if (!catalog_ || preview_entity_ == entt::null || !preview_registry_.valid(preview_entity_)) {
        return;
    }
    if (auto* appearance = preview_registry_.try_get<game::component::AppearanceComponent>(preview_entity_)) {
        applySelectionToComponent(draft_selection_, *appearance);
    }
    game::system::AppearanceLayerCacheBuilder::rebuild(
        preview_registry_,
        preview_entity_,
        *catalog_,
        &context_.getResourceManager());
}

void AppearanceCustomizeScene::updatePreviewPosition() {
    if (preview_entity_ == entt::null || !preview_registry_.valid(preview_entity_)) {
        return;
    }
    auto* transform = preview_registry_.try_get<engine::component::TransformComponent>(preview_entity_);
    auto* render = preview_registry_.try_get<engine::component::RenderComponent>(preview_entity_);
    if (!transform || !render) {
        return;
    }

    const auto& camera = context_.getCamera();
    const glm::vec2 screen_pivot{PREVIEW_SCREEN_PIVOT_X, PREVIEW_SCREEN_PIVOT_Y};
    const glm::vec2 world_position = camera.screenToWorld(screen_pivot);
    const float camera_zoom = camera.getZoom();
    const float screen_space_scale = camera_zoom > 0.0f ? 1.0f / camera_zoom : 1.0f;
    transform->position_ = world_position;
    transform->previous_position_ = world_position;
    transform->scale_ = glm::vec2{screen_space_scale, screen_space_scale};
    render->depth_ = world_position.y;
}

void AppearanceCustomizeScene::focusDefaultAction() {
    auto* document = document_controller_.document();
    if (!document) {
        return;
    }
    if (auto* first_button = document->GetElementById("appearance-random-button")) {
        first_button->Focus(true);
    }
}

void AppearanceCustomizeScene::onSlotStep(int slot_index, int direction) {
    if (slot_index < 0 || static_cast<std::size_t>(slot_index) >= runtime_slots_.size() || !catalog_) {
        return;
    }
    if (!stepAppearanceControl(
            draft_selection_,
            *catalog_,
            runtime_slots_[static_cast<std::size_t>(slot_index)],
            direction,
            mode_ == Mode::NewGame)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
    syncPortraitPreview();
}

void AppearanceCustomizeScene::onRandomize() {
    if (!catalog_ || !randomizeSelection(draft_selection_, *catalog_, rng_, mode_ == Mode::NewGame)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
    syncPortraitPreview();
}

void AppearanceCustomizeScene::onReset() {
    if (!catalog_ || !resetSelectionToProfile(draft_selection_, *catalog_)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
    syncPortraitPreview();
}

void AppearanceCustomizeScene::onConfirm() {
    if (mode_ == Mode::Closet) {
        if (!game_registry_ || !applySelectionToEntity(*game_registry_, context_.getDispatcher(), player_, draft_selection_)) {
            spdlog::warn("AppearanceCustomizeScene: 无法提交衣柜外观。");
        }
        requestPopScene();
        return;
    }

    if (!on_new_game_confirm_) {
        spdlog::warn("AppearanceCustomizeScene: 新游戏确认回调为空。");
        return;
    }
    NewGameCharacterSetup setup{};
    setup.appearance = draft_selection_;
    setup.player_name = normalizePlayerName(player_name_, default_player_name_);
    auto next = on_new_game_confirm_(std::move(setup));
    if (!next) {
        spdlog::warn("AppearanceCustomizeScene: 新游戏确认回调返回空场景。");
        return;
    }
    requestReplaceScene(std::move(next));
}

void AppearanceCustomizeScene::onCancel() {
    if (mode_ == Mode::Closet) {
        requestPopScene();
        return;
    }

    requestPopScene();
}

bool AppearanceCustomizeScene::onMenuCancelPressed() {
    if (mode_ == Mode::NewGame) {
        auto* runtime = context_.getRmlUi();
        auto* rml_context = runtime ? runtime->getContext() : nullptr;
        auto* focused = rml_context ? rml_context->GetFocusElement() : nullptr;
        if (focused && focused->GetId() == "appearance-name-input") {
            focusDefaultAction();
            return true;
        }
    }

    onCancel();
    return true;
}

void AppearanceCustomizeScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    syncLocalizedText(true);
    syncDefaultPlayerName(true);
    syncSlotViewModels(true);
}

void AppearanceCustomizeScene::onSlotPrevEvent(Rml::DataModelHandle,
                                               Rml::Event&,
                                               const Rml::VariantList& arguments) {
    onSlotStep(singleIntArg(arguments), -1);
}

void AppearanceCustomizeScene::onSlotNextEvent(Rml::DataModelHandle,
                                               Rml::Event&,
                                               const Rml::VariantList& arguments) {
    onSlotStep(singleIntArg(arguments), 1);
}

} // namespace game::scene
