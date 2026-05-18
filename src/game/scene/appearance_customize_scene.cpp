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
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/system/appearance_layer_cache_builder.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
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
// Tuned against #appearance-preview-frame in appearance_customize.rcss. The transform
// position is the sprite pivot, so Y sits below the frame's geometric center.
constexpr float PREVIEW_SCREEN_PIVOT_X = 152.0f;
constexpr float PREVIEW_SCREEN_PIVOT_Y = 238.0f;
constexpr float PREVIEW_SIZE_PX = 160.0f;
constexpr float FRAME_SIZE_PX = 32.0f;

[[nodiscard]] Rml::String makeRmlString(std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] int singleIntArg(const Rml::VariantList& arguments) {
    return arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1;
}

[[nodiscard]] bool hasPositiveSize(const engine::utils::Rect& rect) {
    return rect.size.x > 0.0f && rect.size.y > 0.0f;
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
            140.0f);
    }
    return animation;
}

} // namespace

namespace game::scene {

AppearanceCustomizeScene::AppearanceCustomizeScene(std::string_view name,
                                                   engine::core::Context& context,
                                                   SceneFactory on_confirm)
    : engine::scene::Scene(name, context),
      mode_(Mode::NewGame),
      on_new_game_confirm_(std::move(on_confirm)) {
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
      catalog_(std::move(catalog)) {
}

AppearanceCustomizeScene::~AppearanceCustomizeScene() {
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

    runtime_slots_ = runtimeAppearanceSlots(*catalog_);
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

    title_text_ = mode_ == Mode::NewGame ? makeRmlString("Create Hero") : makeRmlString("Wardrobe");
    subtitle_text_ = mode_ == Mode::NewGame ? makeRmlString("Choose a look") : makeRmlString("Change outfit");

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
    if (!constructor.Bind("slots", &slot_view_models_) ||
        !constructor.Bind("title_text", &title_text_) ||
        !constructor.Bind("subtitle_text", &subtitle_text_)) {
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

void AppearanceCustomizeScene::shutdownUI() {
    document_controller_.unload();
}

void AppearanceCustomizeScene::connectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&AppearanceCustomizeScene::onMenuCancelPressed>(this);
}

void AppearanceCustomizeScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&AppearanceCustomizeScene::onMenuCancelPressed>(this);
}

void AppearanceCustomizeScene::syncSlotViewModels(bool mark_dirty) {
    if (!catalog_) {
        return;
    }
    slot_view_models_ = game::ui::buildAppearanceSlotViewModels(*catalog_, draft_selection_);
    if (mark_dirty) {
        document_controller_.markDirty("slots");
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
    if (!stepAppearanceSlot(draft_selection_, *catalog_, runtime_slots_[static_cast<std::size_t>(slot_index)], direction)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
}

void AppearanceCustomizeScene::onRandomize() {
    if (!catalog_ || !randomizeSelection(draft_selection_, *catalog_, rng_)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
}

void AppearanceCustomizeScene::onReset() {
    if (!catalog_ || !resetSelectionToProfile(draft_selection_, *catalog_)) {
        return;
    }
    syncSlotViewModels();
    rebuildPreviewCache();
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
    auto next = on_new_game_confirm_(draft_selection_);
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
    onCancel();
    return true;
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
