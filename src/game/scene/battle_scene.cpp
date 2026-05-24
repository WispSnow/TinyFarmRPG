#include "battle_scene.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/default_resource_ids.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/vfx/vfx_service.h"
#include "engine/vfx/vfx_types.h"
#include "game/battle/battle_ai_planner.h"
#include "game/component/appearance_component.h"
#include "game/defs/events.h"
#include "game/defs/options_events.h"
#include "game/domain/actor_progression_service.h"
#include "game/runtime/user_settings_service.h"
#include "game/scene/battle_cursor_memory.h"
#include "game/scene/battle_scene_data_bindings.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"
#include "game/defs/audio_ids.h"
#include "game/factory/blueprint.h"
#include "game/factory/blueprint_manager.h"
#include "game/system/appearance_layer_cache_builder.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Variant.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/battle.rml";
constexpr std::string_view MODEL_NAME = "battle_scene";
constexpr float BATTLE_CAMERA_ZOOM = 1.0F;
constexpr float BATTLEFIELD_HEIGHT = 256.0f;
constexpr int BATTLE_RENDER_LAYER = 40;
constexpr glm::vec2 COMMAND_FOCUS_PLAYER_OFFSET{-12.0F, -2.0F};
constexpr float COMMAND_FOCUS_EASE_SECONDS = 0.18F;
constexpr glm::vec2 VICTORY_POSE_PLAYER_BASE_OFFSET{-8.0F, -2.0F};
constexpr float VICTORY_POSE_BOB_TAU = 6.28318530717958647692F;
constexpr float VICTORY_POSE_BOB_RATE = 1.2F;
constexpr float VICTORY_POSE_BOB_PIXELS = 2.0F;
constexpr float BATTLE_SHADOW_VERTICAL_PADDING = -15.0F;
constexpr float BATTLE_SHADOW_ALPHA = 0.4F;
constexpr float BATTLE_SHADOW_DEPTH_OFFSET = -0.10F;
constexpr float BATTLE_TARGET_SHADOW_DEPTH_OFFSET = -0.05F;
constexpr int DAMAGE_POPUP_FONT_SIZE_PX = 20;
constexpr float HP_BAR_WARNING_RATIO = 0.50F;
constexpr float HP_BAR_DANGER_RATIO = 0.25F;
constexpr std::size_t BATTLE_LOG_HISTORY_LIMIT = 24U;
constexpr std::size_t BATTLE_LOG_VISIBLE_LIMIT = 3U;
constexpr std::string_view BASIC_ATTACK_SKILL_ID = "skill.attack";

enum class PartyCommandId : int {
    Fight = 1,
    Escape = 2
};

enum class ActorCommandId : int {
    Attack = 1,
    Skill = 2,
    Guard = 3,
    Item = 4
};

struct BattleSpriteComponent {
    game::battle::BattleUnitId unit_id{0};
    game::battle::BattleSide side{game::battle::BattleSide::Player};
    glm::vec2 screen_position{0.0f};
    float scale{1.0f};
    float depth{0.0f};
    glm::vec2 shadow_size{32.0f, 16.0f};
    glm::vec2 shadow_offset{0.0f, 0.0f};
    entt::entity shadow_entity{entt::null};
    entt::entity target_shadow_entity{entt::null};

    BattleSpriteComponent() = default;
    BattleSpriteComponent(game::battle::BattleUnitId unit_id,
                          game::battle::BattleSide side,
                          glm::vec2 screen_position,
                          float scale,
                          float depth,
                          glm::vec2 shadow_size,
                          glm::vec2 shadow_offset)
        : unit_id(unit_id),
          side(side),
          screen_position(screen_position),
          scale(scale),
          depth(depth),
          shadow_size(shadow_size),
          shadow_offset(shadow_offset) {}
};

struct BattleShadowComponent {
    game::battle::BattleUnitId owner_unit_id{0};
    bool target_highlight{false};
};

struct BattleFormationSlot {
    glm::vec2 screen_position{0.0F};
    float scale{1.0F};
    float depth{0.0F};
    glm::vec2 shadow_size{56.0F, 4.0F};
};

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;
using namespace entt::literals;

[[nodiscard]] int getSingleIntArgument(const Rml::VariantList& arguments) {
    if (arguments.size() != 1) {
        return -1;
    }

    return arguments[0].Get<int>(-1);
}

[[nodiscard]] Rml::String makeElementId(std::string_view prefix, int index) {
    Rml::String element_id{prefix.data(), prefix.size()};
    element_id += std::to_string(index);
    return element_id;
}

[[nodiscard]] Rml::String makeRmlString(std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] entt::id_type hashString(std::string_view value) {
    return entt::hashed_string{value.data(), value.size()}.value();
}

[[nodiscard]] const game::battle::BattleUnit* findBattleUnitById(const std::vector<game::battle::BattleUnit>& units,
                                                                 const game::battle::BattleUnitId unit_id) {
    const auto it = std::find_if(units.begin(), units.end(), [unit_id](const game::battle::BattleUnit& unit) {
        return unit.id == unit_id;
    });
    return it != units.end() ? &*it : nullptr;
}

/// @brief 普通攻击由 ActorCommandId::Attack 承担，不作为 Skill 菜单条目展示。
[[nodiscard]] bool isActorSkillMenuEntry(std::string_view skill_id) {
    return skill_id != BASIC_ATTACK_SKILL_ID;
}

[[nodiscard]] engine::component::Animation toRuntimeAnimation(const game::factory::AnimationBlueprint& blueprint,
                                                              bool loop = true) {
    std::vector<engine::component::AnimationFrame> frames;
    frames.reserve(blueprint.frames_.size());
    for (const int frame_index : blueprint.frames_) {
        engine::utils::Rect source_rect{blueprint.position_, blueprint.src_size_};
        source_rect.pos.x += static_cast<float>(frame_index) * blueprint.src_size_.x;
        frames.emplace_back(source_rect, blueprint.ms_per_frame_);
    }

    engine::component::Animation animation{};
    animation.name_ = blueprint.name_;
    animation.texture_id_ = blueprint.texture_id_;
    animation.pivot_ = blueprint.pivot_;
    animation.dst_size_ = blueprint.dst_size_;
    animation.frames_ = std::move(frames);
    animation.events_ = blueprint.events_;
    animation.loop_ = loop;
    animation.flip_horizontal_ = blueprint.flip_horizontal_;
    return animation;
}

[[nodiscard]] std::unordered_map<entt::id_type, engine::component::Animation>
toRuntimeAnimations(const std::unordered_map<entt::id_type, game::factory::AnimationBlueprint>& blueprints) {
    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.reserve(blueprints.size());
    for (const auto& [animation_id, blueprint] : blueprints) {
        animations.emplace(animation_id, toRuntimeAnimation(blueprint));
    }
    return animations;
}

void applyAnimationFrame(engine::component::AnimationComponent& animation,
                         engine::component::SpriteComponent& sprite) {
    const auto animation_it = animation.animations_.find(animation.current_animation_id_);
    if (animation_it == animation.animations_.end()) {
        return;
    }

    const auto& current_animation = animation_it->second;
    if (current_animation.frames_.empty()) {
        return;
    }

    animation.current_frame_index_ = std::min(animation.current_frame_index_, current_animation.frames_.size() - 1);
    const auto& frame = current_animation.frames_[animation.current_frame_index_];
    sprite.sprite_.texture_id_ = current_animation.texture_id_;
    sprite.sprite_.src_rect_ = frame.src_rect_;
    sprite.sprite_.is_flipped_ = current_animation.flip_horizontal_;
    if (current_animation.dst_size_.x > 0.0f && current_animation.dst_size_.y > 0.0f) {
        sprite.size_ = current_animation.dst_size_;
    }
    sprite.pivot_ = current_animation.pivot_;
}

void advanceAnimation(engine::component::AnimationComponent& animation,
                      engine::component::SpriteComponent& sprite,
                      float delta_time) {
    const auto animation_it = animation.animations_.find(animation.current_animation_id_);
    if (animation_it == animation.animations_.end()) {
        return;
    }

    const auto& current_animation = animation_it->second;
    if (current_animation.frames_.empty()) {
        return;
    }

    animation.current_time_ms_ += delta_time * 1000.0f * animation.speed_;
    while (animation.current_frame_index_ < current_animation.frames_.size()) {
        const float duration_ms = std::max(1.0f, current_animation.frames_[animation.current_frame_index_].duration_ms_);
        if (animation.current_time_ms_ < duration_ms) {
            break;
        }

        animation.current_time_ms_ -= duration_ms;
        ++animation.current_frame_index_;
        if (animation.current_frame_index_ < current_animation.frames_.size()) {
            continue;
        }

        if (current_animation.loop_) {
            animation.current_frame_index_ = 0;
        } else {
            animation.current_frame_index_ = current_animation.frames_.size() - 1;
            animation.current_time_ms_ = 0.0f;
            break;
        }
    }

    applyAnimationFrame(animation, sprite);
}

[[nodiscard]] BattleFormationSlot battleFormationSlot(game::battle::BattleSide side,
                                                      std::size_t side_index,
                                                      std::size_t side_count,
                                                      float visual_scale) {
    const float centered = static_cast<float>(side_index) - (static_cast<float>(side_count) - 1.0F) * 0.5F;
    const bool is_player = side == game::battle::BattleSide::Player;
    const glm::vec2 base = is_player ? glm::vec2{480.0F, 172.0F} : glm::vec2{160.0F, 172.0F};
    const glm::vec2 step = is_player ? glm::vec2{18.0F, 28.0F} : glm::vec2{-18.0F, 30.0F};
    glm::vec2 position = base + centered * step;
    position.y = std::clamp(position.y, 96.0F, BATTLEFIELD_HEIGHT - 30.0F);

    const float shadow_width = std::clamp(16.0F * visual_scale, 12.0F, 42.0F);
    return BattleFormationSlot{
        .screen_position = position,
        .scale = visual_scale,
        .depth = position.y,
        .shadow_size = glm::vec2{shadow_width, std::clamp(6.0F * visual_scale, 3.0F, 24.0F)}
    };
}

[[nodiscard]] engine::utils::FColor multiplyColor(const engine::utils::FColor& lhs,
                                                  const engine::utils::FColor& rhs) {
    return engine::utils::FColor{
        std::clamp(lhs.r * rhs.r, 0.0F, 1.6F),
        std::clamp(lhs.g * rhs.g, 0.0F, 1.6F),
        std::clamp(lhs.b * rhs.b, 0.0F, 1.6F),
        std::clamp(lhs.a * rhs.a, 0.0F, 1.0F)};
}

[[nodiscard]] engine::utils::Rect screenRectToWorldRect(const engine::render::Camera& camera,
                                                        const glm::vec2& position,
                                                        const glm::vec2& size) {
    const glm::vec2 top_left = camera.screenToWorld(position);
    const glm::vec2 bottom_right = camera.screenToWorld(position + size);
    return engine::utils::Rect{top_left, bottom_right - top_left};
}

[[nodiscard]] glm::vec2 battleShadowScreenPosition(const BattleSpriteComponent& sprite,
                                                   const engine::component::SpriteComponent& visual) {
    const glm::vec2 visual_size = visual.size_ * sprite.scale;
    const float foot_y = (1.0F - visual.pivot_.y) * visual_size.y + BATTLE_SHADOW_VERTICAL_PADDING;
    return sprite.screen_position + glm::vec2{0.0F, foot_y} + sprite.shadow_offset;
}

[[nodiscard]] engine::utils::FColor enemyHpBarFillColor(const float ratio, const float alpha) {
    const float clamped_alpha = std::clamp(alpha, 0.0F, 1.0F);
    if (ratio <= HP_BAR_DANGER_RATIO) {
        return engine::utils::FColor{0.95F, 0.18F, 0.14F, clamped_alpha};
    }
    if (ratio <= HP_BAR_WARNING_RATIO) {
        return engine::utils::FColor{0.98F, 0.78F, 0.22F, clamped_alpha};
    }
    return engine::utils::FColor{0.22F, 0.86F, 0.32F, clamped_alpha};
}

[[nodiscard]] glm::vec2 enemyHpBarScreenTopLeft(const BattleSpriteComponent& battle_sprite,
                                                const engine::component::SpriteComponent& visual,
                                                const game::scene::BattleEnemyHpBarConfig& config) {
    const glm::vec2 visual_size = visual.size_ * battle_sprite.scale;
    const float sprite_top_y = battle_sprite.screen_position.y - visual.pivot_.y * visual_size.y;
    const glm::vec2 center{
        battle_sprite.screen_position.x,
        sprite_top_y - config.above_sprite_margin - config.size.y * 0.5F};
    return center - config.size * 0.5F;
}

} // namespace

namespace game::scene {

BattleScene::BattleScene(std::string_view name,
                         engine::core::Context& context,
                         std::vector<game::battle::BattleUnit> units,
                         game::battle::BattleSessionOptions session_options,
                         BattleScenePresentationOptions presentation_options)
    : engine::scene::Scene(name, context),
      rpg_catalog_(session_options.rpg_catalog),
      item_catalog_(session_options.item_catalog),
      blueprint_manager_(presentation_options.blueprint_manager),
      appearance_catalog_(presentation_options.appearance_catalog),
      vfx_service_(presentation_options.vfx_service),
      view_model_builder_(rpg_catalog_, item_catalog_, blueprint_manager_),
      session_(std::move(units), std::move(session_options)),
      presentation_options_(std::move(presentation_options)),
      battle_enemy_hp_bar_controller_(presentation_options_.enemy_hp_bar_config) {
}

BattleScene::~BattleScene() {
    disconnectInputListeners();
    shutdownUI();
    restoreBattleCamera();
}

bool BattleScene::init() {
    enterBattleCamera();
    context_.getInputManager().pushContext(engine::input::InputContextId::Battle);
    context_pushed_ = true;

    if (!initUI()) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
        restoreBattleCamera();
        return false;
    }

    if (!initPresentation()) {
        shutdownUI();
        context_.getInputManager().popContext();
        context_pushed_ = false;
        restoreBattleCamera();
        return false;
    }

    if (!Scene::init()) {
        shutdownUI();
        context_.getInputManager().popContext();
        context_pushed_ = false;
        restoreBattleCamera();
        return false;
    }

    connectInputListeners();
    syncUserSettingsState();
    connectUserSettingsListeners();

    if (session_.outcome() == game::battle::BattleOutcome::Victory) {
        flow_controller_.startVictoryFlow(*this);
    } else if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        flow_controller_.setBattleEnd();
        leaveInputMenu();
    } else {
        beginCurrentTurnFlow();
    }
    refreshView();
    return true;
}

void BattleScene::update(float delta_time) {
    Scene::update(delta_time);
    updatePresentation(delta_time);
    battle_enemy_hp_bar_controller_.update(delta_time);
    battle_damage_popup_controller_.update(delta_time);
    runStateMachine(delta_time);
    updateScheduledPresentationEvents(delta_time);
    if (vfx_service_) {
        vfx_service_->update(delta_time);
    }
    updateCommandFocus(delta_time);
    refreshView();
}

void BattleScene::render(float interpolation_alpha) {
    Scene::render(interpolation_alpha);
    context_.getRenderer().beginFrame(context_.getCamera());
    renderBattlefieldBackground();
    refreshPresentation();
    syncPresentationTransforms();
    syncPresentationShadows();
    battle_render_system_.renderPrepared(battle_registry_, context_.getRenderer(), interpolation_alpha);
    renderEnemyHpBars();
    renderDamagePopups();
}

void BattleScene::prepareUi(float interpolation_alpha) {
    Scene::prepareUi(interpolation_alpha);
    syncMenuFocus();
    syncVictoryContinueFocus();
}

void BattleScene::clean() {
    disconnectUserSettingsListeners();
    disconnectInputListeners();
    shutdownUI();
    battle_animation_director_.reset();
    battle_damage_popup_controller_.clear();
    battle_enemy_hp_bar_controller_.clear();
    scheduled_presentation_events_.clear();
    battle_log_history_.clear();
    battle_log_entries_.clear();
    last_actor_command_index_per_actor_.clear();
    last_skill_id_per_actor_.clear();
    last_item_id_per_actor_.clear();
    last_target_unit_id_per_actor_.clear();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    restoreBattleCamera();
    Scene::clean();
}

engine::scene::SceneUiCoverage BattleScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
}

bool BattleScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("BattleScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("BattleScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("BattleScene: 注册菜单 data types 失败。");
        document_controller_.unload();
        return false;
    }

    populatePartyCommands();
    populateActorCommands();

    if (!menu_model_.bind(constructor) ||
        !constructor.Bind("victory_overlay_visible", &victory_overlay_visible_) ||
        !constructor.Bind("victory_continue_enabled", &victory_continue_enabled_) ||
        !constructor.Bind("victory_items_empty", &victory_items_empty_) ||
        !constructor.Bind("victory_level_ups_empty", &victory_level_ups_empty_) ||
        !constructor.Bind("victory_title", &victory_title_) ||
        !constructor.Bind("victory_gold_text", &victory_gold_text_) ||
        !constructor.Bind("victory_exp_text", &victory_exp_text_) ||
        !constructor.Bind("victory_item_empty_text", &victory_item_empty_text_) ||
        !constructor.Bind("victory_prompt_text", &victory_prompt_text_) ||
        !constructor.Bind("turn_order_entries", &turn_order_entries_) ||
        !constructor.Bind("party_status", &party_status_) ||
        !constructor.Bind("party_state_icons", &party_state_icons_) ||
        !constructor.Bind("state_tooltip", &state_tooltip_) ||
        !constructor.Bind("battle_log_entries", &battle_log_entries_) ||
        !constructor.Bind("victory_reward_items", &victory_reward_items_) ||
        !constructor.Bind("victory_level_ups", &victory_level_ups_)) {
        spdlog::error("BattleScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(
            constructor,
            "victory_continue",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (flow_controller_.isVictoryFlow()) {
                    victory_flow_controller_.confirm();
                }
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "party_command_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handlePartyCommand(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "actor_command_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleActorCommand(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "list_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleListEntry(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "target_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleTargetEntry(getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "state_icon_hover_enter",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                if (arguments.size() != 2) {
                    return;
                }
                handleStateIconHoverEnter(arguments[0].Get<int>(-1), arguments[1].Get<int>(-1));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "state_icon_hover_exit",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                if (arguments.size() != 2) {
                    return;
                }
                handleStateIconHoverExit(arguments[0].Get<int>(-1), arguments[1].Get<int>(-1));
            })) {
        spdlog::error("BattleScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("BattleScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    document_controller_.markAllDirty();
    menu_model_.focus_dirty = true;
    return true;
}

void BattleScene::shutdownUI() {
    document_controller_.unload();
}

bool BattleScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (!registerBattleSceneViewModelStructs(constructor)) {
        return false;
    }

    if (!menu_model_.registerArrays(constructor) ||
        !constructor.RegisterArray<decltype(turn_order_entries_)>() ||
        !constructor.RegisterArray<decltype(party_status_)>() ||
        !constructor.RegisterArray<decltype(party_state_icons_)>() ||
        !constructor.RegisterArray<decltype(battle_log_entries_)>() ||
        !constructor.RegisterArray<decltype(victory_reward_items_)>() ||
        !constructor.RegisterArray<decltype(victory_level_ups_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void BattleScene::connectInputListeners() {
    input_router_.connect(context_.getInputManager(), *this);
}

void BattleScene::disconnectInputListeners() {
    input_router_.disconnect();
}

void BattleScene::enterBattleCamera() {
    if (saved_camera_state_) {
        return;
    }

    auto& camera = context_.getCamera();
    saved_camera_state_ = CameraStateSnapshot{
        .position = camera.getPosition(),
        .zoom = camera.getZoom(),
        .rotation = camera.getRotation(),
        .min_zoom = camera.getMinZoom(),
        .max_zoom = camera.getMaxZoom(),
        .limit_bounds = camera.getLimitBounds(),
    };

    camera.setLimitBounds(std::nullopt);
    camera.setRotation(0.0F);
    camera.setMinZoom(BATTLE_CAMERA_ZOOM);
    camera.setMaxZoom(BATTLE_CAMERA_ZOOM);
    camera.setPosition(camera.getLogicalSize() * 0.5F);
    camera.setZoom(BATTLE_CAMERA_ZOOM);
}

void BattleScene::restoreBattleCamera() {
    if (!saved_camera_state_) {
        return;
    }

    auto& camera = context_.getCamera();
    const CameraStateSnapshot snapshot = *saved_camera_state_;
    saved_camera_state_.reset();

    camera.setLimitBounds(std::nullopt);
    camera.setMaxZoom(snapshot.max_zoom);
    camera.setMinZoom(snapshot.min_zoom);
    camera.setRotation(snapshot.rotation);
    camera.setZoom(snapshot.zoom);
    camera.setLimitBounds(snapshot.limit_bounds);
    camera.setPosition(snapshot.position);
}

void BattleScene::runStateMachine(float delta_time) {
    flow_controller_.run(delta_time, *this);
}

bool BattleScene::hasPendingAction() const {
    return pending_action_.has_value();
}

void BattleScene::executePendingAction() {
    if (!pending_action_) {
        return;
    }

    const auto before_units = session_.units();
    const std::uint32_t round_index = session_.roundIndex();
    last_action_result_ = session_.submitAction(*pending_action_);
    emitBattleActionScriptEvents(*last_action_result_, before_units, round_index);
    appendBattleLogLines(game::battle::formatBattleLogLines(
        *last_action_result_,
        game::battle::BattleLogFormatterContext{
            .rpg_catalog = rpg_catalog_,
            .item_catalog = item_catalog_
        }));
    const auto unit_anchors = collectBattlePresentationUnitAnchors();
    const BattleActionPresentationPlan presentation_plan =
        presentationPlanForResult(*last_action_result_, unit_anchors);
    const BattleAnimationTimelineConfig animation_config = animationConfigForPlan(presentation_plan);
    battle_enemy_hp_bar_controller_.stageSnapshot(last_action_result_->snapshot);
    schedulePresentationPlanEvents(presentation_plan, *last_action_result_);
    battle_damage_popup_controller_.spawnFromResult(
        *last_action_result_,
        unit_anchors,
        presentation_plan.impact_time_seconds);
    battle_animation_director_.begin(*last_action_result_, unit_anchors, animation_config);
    pending_action_.reset();
}

void BattleScene::beginCurrentTurnFlow() {
    pending_action_.reset();

    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        flow_controller_.setBattleEnd();
        leaveInputMenu();
        return;
    }

    const auto* actor = currentActor();
    if (!actor) {
        flow_controller_.setBattleEnd();
        leaveInputMenu();
        return;
    }

    emitBattleTurnStarted(*actor);

    if (actor->side == game::battle::BattleSide::Enemy) {
        submitAction(buildEnemyAction(*actor));
        return;
    }

    flow_controller_.waitForInput();
    enterInputMenu();
}

const game::battle::BattleUnit* BattleScene::currentActor() const {
    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return nullptr;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id.has_value()) {
        return nullptr;
    }

    return session_.findUnit(*actor_id);
}

void BattleScene::emitBattleTurnStarted(const game::battle::BattleUnit& unit) {
    context_.getDispatcher().trigger(game::defs::BattleTurnStartedEvent{
        .unit = unit,
        .round_index = session_.roundIndex(),
    });
}

void BattleScene::emitBattleActionScriptEvents(const game::battle::BattleActionResult& result,
                                               const std::vector<game::battle::BattleUnit>& before_units,
                                               const std::uint32_t round_index) {
    if (result.status != game::battle::BattleActionStatus::Applied) {
        return;
    }

    const auto* actor_after = findBattleUnitById(result.snapshot.units, result.actor_id);
    const auto* actor_before = findBattleUnitById(before_units, result.actor_id);
    const auto* actor = actor_after ? actor_after : actor_before;
    if (actor) {
        context_.getDispatcher().trigger(game::defs::BattleTurnEndedEvent{
            .unit = *actor,
            .result = result,
            .round_index = round_index,
        });

        if (result.action_type == game::battle::BattleActionType::Skill) {
            context_.getDispatcher().trigger(game::defs::BattleSkillUsedEvent{
                .unit = *actor,
                .result = result,
                .round_index = round_index,
            });
        }
    }

    for (const auto& before_unit : before_units) {
        if (!before_unit.isAlive()) {
            continue;
        }

        const auto* after_unit = findBattleUnitById(result.snapshot.units, before_unit.id);
        if (!after_unit || after_unit->isAlive()) {
            continue;
        }

        context_.getDispatcher().trigger(game::defs::BattleUnitDiedEvent{
            .unit = *after_unit,
            .source_unit_id = result.actor_id,
            .source_action_type = result.action_type,
            .skill_id = result.skill_id,
            .item_id = result.item_id,
            .round_index = round_index,
        });
    }
}

game::battle::BattleAction BattleScene::buildEnemyAction(const game::battle::BattleUnit& actor) const {
    const auto fallback_action = game::battle::BattleAiPlanner::planFallbackAction(actor, session_.units());

    if (actor.side != game::battle::BattleSide::Enemy) {
        return fallback_action;
    }

    if (!rpg_catalog_) {
        spdlog::warn("BattleScene: enemy actor '{}' 缺少 RPG catalog，回退为基础行动。", actor.name);
        return fallback_action;
    }

    if (!actor.source_enemy_id.has_value()) {
        spdlog::warn("BattleScene: enemy actor '{}' 缺少 source_enemy_id，回退为基础行动。", actor.name);
        return fallback_action;
    }

    const auto* enemy = rpg_catalog_->findEnemy(*actor.source_enemy_id);
    if (!enemy) {
        spdlog::warn("BattleScene: enemy source '{}' 不存在于 RPG catalog，回退为基础行动。", *actor.source_enemy_id);
        return fallback_action;
    }

    return game::battle::BattleAiPlanner::planEnemyAction(actor, *enemy, session_.units(), *rpg_catalog_);
}

void BattleScene::updateResultAnimation(const float delta_time) {
    battle_animation_director_.update(delta_time);
}

bool BattleScene::resultAnimationFinished() const {
    return battle_animation_director_.finished();
}

game::battle::BattleOutcome BattleScene::battleOutcome() const {
    return session_.outcome();
}

void BattleScene::refreshView() {
    const auto current_actor_id = session_.currentActorId();

    std::string turn_text = "Turn: -";
    if (current_actor_id) {
        if (const auto* actor = session_.findUnit(*current_actor_id)) {
            turn_text = "Turn: " + actor->name + " (" + std::string(game::battle::toString(actor->side)) + ")";
        }
    }
    if (updateBoundString(menu_model_.turn_text, turn_text)) {
        document_controller_.markDirty("turn_text");
    }

    rebuildTurnOrderView();
    rebuildPartyStatusView();
    rebuildVictoryView();

    std::string result_text = "Result: " + menu_model_.status_text;
    if (flow_controller_.isVictoryFlow()) {
        result_text = "Result: Victory";
    } else if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        result_text = "Result: " + std::string(game::battle::toString(session_.outcome()));
    }
    if (updateBoundString(menu_model_.result_text, result_text)) {
        document_controller_.markDirty("result_text");
    }

    const bool can_submit_action =
        !end_requested_ &&
        flow_controller_.isWaitingForInput() &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        current_actor_id.has_value();

    if (updateBoundBool(menu_model_.actions_enabled, can_submit_action)) {
        document_controller_.markDirty("actions_enabled");
    }

    refreshMenuEnabledState(can_submit_action);
    if (!can_submit_action && menu_model_.state != MenuState::None) {
        leaveInputMenu();
    } else if (can_submit_action && menu_model_.state == MenuState::None) {
        enterInputMenu();
    }
}

void BattleScene::rebuildVictoryView() {
    const BattleVictoryFlowSnapshot snapshot = victory_flow_controller_.snapshot();
    const bool overlay_visible = flow_controller_.isVictoryFlow() && victory_flow_controller_.active();
    if (updateBoundBool(victory_overlay_visible_, overlay_visible)) {
        document_controller_.markDirty("victory_overlay_visible");
    }

    const bool continue_enabled = overlay_visible && snapshot.waiting_for_confirm;
    if (updateBoundBool(victory_continue_enabled_, continue_enabled)) {
        document_controller_.markDirty("victory_continue_enabled");
        if (continue_enabled) {
            victory_continue_focus_dirty_ = true;
        }
    }

    const std::string gold_text = std::to_string(std::max(0, snapshot.gold.display));
    if (updateBoundString(victory_gold_text_, gold_text)) {
        document_controller_.markDirty("victory_gold_text");
    }

    const std::string exp_text = std::to_string(std::max(0, snapshot.exp.display));
    if (updateBoundString(victory_exp_text_, exp_text)) {
        document_controller_.markDirty("victory_exp_text");
    }

    const std::string prompt_text = snapshot.waiting_for_confirm ? "Continue" : "Confirm";
    if (updateBoundString(victory_prompt_text_, prompt_text)) {
        document_controller_.markDirty("victory_prompt_text");
    }

    std::vector<VictoryRewardItemViewModel> next_items = view_model_builder_.buildVictoryRewardItems(snapshot);

    if (victory_reward_items_ != next_items) {
        victory_reward_items_ = std::move(next_items);
        document_controller_.markDirty("victory_reward_items");
    }

    if (updateBoundBool(victory_items_empty_, victory_reward_items_.empty())) {
        document_controller_.markDirty("victory_items_empty");
    }

    std::vector<VictoryLevelUpViewModel> next_level_ups = view_model_builder_.buildVictoryLevelUps(snapshot);

    if (victory_level_ups_ != next_level_ups) {
        victory_level_ups_ = std::move(next_level_ups);
        document_controller_.markDirty("victory_level_ups");
    }

    if (updateBoundBool(victory_level_ups_empty_, victory_level_ups_.empty())) {
        document_controller_.markDirty("victory_level_ups_empty");
    }

    const std::string empty_text = snapshot.waiting_for_confirm ? "No drops" : "Resolving...";
    if (updateBoundString(victory_item_empty_text_, empty_text)) {
        document_controller_.markDirty("victory_item_empty_text");
    }
}

void BattleScene::rebuildTurnOrderView() {
    std::vector<TurnOrderEntryViewModel> next_turn_order_entries =
        view_model_builder_.buildTurnOrderEntries(session_);
    if (turn_order_entries_ != next_turn_order_entries) {
        turn_order_entries_ = std::move(next_turn_order_entries);
        document_controller_.markDirty("turn_order_entries");
    }
}

void BattleScene::rebuildPartyStatusView() {
    BattlePartyHudViewModels next_hud = view_model_builder_.buildPartyHud(session_);
    const std::vector<PartyStatusViewModel>& next_party_status = next_hud.party_status;
    const std::vector<StateIconViewModel>& next_party_state_icons = next_hud.party_state_icons;

    if (party_status_.size() != next_party_status.size() ||
        !std::equal(party_status_.begin(),
                    party_status_.end(),
                    next_party_status.begin(),
                    [](const PartyStatusViewModel& lhs, const PartyStatusViewModel& rhs) {
                        return lhs.unit_id == rhs.unit_id &&
                            lhs.name == rhs.name &&
                            lhs.hp_text == rhs.hp_text &&
                            lhs.mp_text == rhs.mp_text &&
                            lhs.hp_ratio_percent == rhs.hp_ratio_percent &&
                            lhs.mp_ratio_percent == rhs.mp_ratio_percent &&
                            lhs.portrait_decorator == rhs.portrait_decorator &&
                            lhs.active == rhs.active &&
                            lhs.ko == rhs.ko;
                    })) {
        party_status_ = std::move(next_hud.party_status);
        document_controller_.markDirty("party_status");
    }

    if (party_state_icons_ != next_party_state_icons) {
        party_state_icons_ = std::move(next_hud.party_state_icons);
        document_controller_.markDirty("party_state_icons");
    }

    if (state_tooltip_.visible) {
        const bool tooltip_target_exists = std::any_of(
            party_state_icons_.begin(),
            party_state_icons_.end(),
            [this](const StateIconViewModel& icon) {
                return icon.unit_id == state_tooltip_.active_unit_id &&
                    icon.entry_index == state_tooltip_entry_index_;
            });
        if (!tooltip_target_exists) {
            hideStateTooltip();
        }
    }
}

void BattleScene::hideStateTooltip() {
    const StateTooltipViewModel next_tooltip{};
    if (state_tooltip_ != next_tooltip || state_tooltip_entry_index_ != -1) {
        state_tooltip_ = next_tooltip;
        state_tooltip_entry_index_ = -1;
        document_controller_.markDirty("state_tooltip");
    }
}

void BattleScene::appendBattleLogLines(const std::vector<game::battle::BattleLogLine>& lines) {
    if (lines.empty()) {
        return;
    }

    battle_log_history_.insert(battle_log_history_.end(), lines.begin(), lines.end());
    if (battle_log_history_.size() > BATTLE_LOG_HISTORY_LIMIT) {
        const auto erase_count = static_cast<std::ptrdiff_t>(battle_log_history_.size() - BATTLE_LOG_HISTORY_LIMIT);
        battle_log_history_.erase(battle_log_history_.begin(), battle_log_history_.begin() + erase_count);
    }
    rebuildBattleLogView();
}

void BattleScene::rebuildBattleLogView() {
    std::vector<BattleLogEntryViewModel> next_entries =
        view_model_builder_.buildBattleLogEntries(battle_log_history_, BATTLE_LOG_VISIBLE_LIMIT);

    if (battle_log_entries_ != next_entries) {
        battle_log_entries_ = std::move(next_entries);
        document_controller_.markDirty("battle_log_entries");
    }
}

void BattleScene::refreshMenuEnabledState(bool enabled) {
    menu_model_.refreshCommandEnabled(enabled, document_controller_);
}

void BattleScene::markMenuDirty() {
    menu_model_.markDirty(document_controller_);
}

void BattleScene::enterInputMenu() {
    action_draft_ = {};
    if (shouldOpenPartyCommand()) {
        setMenuState(MenuState::PartyCommand);
    } else {
        setMenuState(MenuState::ActorCommand);
    }
}

void BattleScene::leaveInputMenu() {
    action_draft_ = {};
    actor_command_entered_via_fight_this_step_ = false;
    setMenuState(MenuState::None);
}

void BattleScene::setMenuState(MenuState next_state) {
    menu_model_.setState(next_state, document_controller_);
    syncEnemyHpBarHighlight();
}

void BattleScene::syncMenuFocus() {
    if (!menu_model_.focus_dirty) {
        return;
    }

    int cursor = -1;
    std::string_view prefix;

    switch (menu_model_.state) {
        case MenuState::None:
            menu_model_.focus_dirty = false;
            return;
        case MenuState::PartyCommand:
            cursor = menu_model_.party_command_cursor;
            prefix = "battle-party-command-";
            break;
        case MenuState::ActorCommand:
            cursor = menu_model_.actor_command_cursor;
            prefix = "battle-actor-command-";
            break;
        case MenuState::SkillList:
        case MenuState::ItemList:
            cursor = menu_model_.list_entry_cursor;
            prefix = "battle-list-entry-";
            break;
        case MenuState::TargetSelect:
            cursor = menu_model_.target_entry_cursor;
            prefix = "battle-target-entry-";
            break;
    }

    // cursor < 0: 无可聚焦条目，直接清除脏标记。
    // cursor >= 0 且 focus 成功: 清除。
    // cursor >= 0 但元素尚未生成（data-if 子树未展开）: 保持脏标记，下帧重试。
    if (cursor < 0 || focusElementById(makeElementId(prefix, cursor))) {
        menu_model_.focus_dirty = false;
    }
}

void BattleScene::syncVictoryContinueFocus() {
    if (!victory_continue_focus_dirty_) {
        return;
    }

    // focus 可能早于 RmlUi disabled 属性同步；失败时保留脏标记，下帧重试。
    if (!victory_continue_enabled_ || focusElementById("battle-victory-continue")) {
        victory_continue_focus_dirty_ = false;
    }
}

bool BattleScene::focusElementById(std::string_view element_id) {
    auto* document = document_controller_.document();
    if (!document) {
        return false;
    }

    auto* element = document->GetElementById(Rml::String{element_id.data(), element_id.size()});
    if (!element) {
        return false;
    }

    element->Focus(true);
    element->ScrollIntoView(Rml::ScrollIntoViewOptions{
        Rml::ScrollAlignment::Nearest,
        Rml::ScrollAlignment::Nearest,
        Rml::ScrollBehavior::Instant,
        Rml::ScrollParentage::Closest
    });
    return true;
}

void BattleScene::populatePartyCommands() {
    const bool enabled = menu_model_.actions_enabled;
    menu_model_.party_commands = {
        CommandViewModel{.command_id = static_cast<int>(PartyCommandId::Fight), .entry_index = 0, .label = "Fight", .enabled = enabled},
        CommandViewModel{.command_id = static_cast<int>(PartyCommandId::Escape), .entry_index = 1, .label = "Escape", .enabled = enabled},
    };
    menu_model_.party_command_cursor = firstEnabledPartyCommandIndex();
}

void BattleScene::populateActorCommands() {
    const bool enabled = menu_model_.actions_enabled;
    menu_model_.actor_commands = {
        CommandViewModel{.command_id = static_cast<int>(ActorCommandId::Attack), .entry_index = 0, .label = "Attack", .enabled = enabled},
        CommandViewModel{.command_id = static_cast<int>(ActorCommandId::Skill), .entry_index = 1, .label = "Skill", .enabled = enabled},
        CommandViewModel{.command_id = static_cast<int>(ActorCommandId::Guard), .entry_index = 2, .label = "Guard", .enabled = enabled},
        CommandViewModel{.command_id = static_cast<int>(ActorCommandId::Item), .entry_index = 3, .label = "Item", .enabled = enabled},
    };

    const int fallback = firstEnabledActorCommandIndex();
    int remembered = -1;
    if (const auto* actor = currentActor()) {
        if (const auto it = last_actor_command_index_per_actor_.find(actor->id);
            it != last_actor_command_index_per_actor_.end()) {
            remembered = it->second;
        }
    }
    std::vector<bool> enabled_states;
    enabled_states.reserve(menu_model_.actor_commands.size());
    for (const auto& cmd : menu_model_.actor_commands) {
        enabled_states.push_back(cmd.enabled);
    }
    menu_model_.actor_command_cursor = resolveCursorMemoryDefaultIndex(
        remembered, enabled_states, fallback, cursor_memory_enabled_);
}

void BattleScene::populateSkillEntries(const game::battle::BattleUnit& actor) {
    menu_model_.list_entries.clear();
    menu_model_.list_entry_cursor = -1;
    menu_model_.list_empty_text = "No skills available";

    if (!rpg_catalog_) {
        spdlog::warn("BattleScene: RPG catalog 不可用，无法生成技能列表。");
        return;
    }

    int entry_index = 0;
    for (const auto& skill_id : actor.skill_ids) {
        if (!isActorSkillMenuEntry(skill_id)) {
            continue;
        }

        const auto* skill = rpg_catalog_->findSkill(skill_id);
        if (!skill) {
            spdlog::warn("BattleScene: skill '{}' 不存在于 RPG catalog，已跳过。", skill_id);
            continue;
        }

        const std::string_view label = skill->display_name_.empty()
            ? std::string_view{skill->id_}
            : std::string_view{skill->display_name_};
        menu_model_.list_entries.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = skill->id_,
            .label = makeRmlString(label),
            .sublabel = skillSubtitle(actor, *skill),
            .enabled = isSkillEntryEnabled(actor, *skill)
        });
    }

    const int fallback = firstEnabledListEntryIndex();
    int remembered = -1;
    if (cursor_memory_enabled_) {
        if (const auto it = last_skill_id_per_actor_.find(actor.id); it != last_skill_id_per_actor_.end()) {
            for (const auto& entry : menu_model_.list_entries) {
                if (entry.entry_id == it->second) {
                    remembered = entry.entry_index;
                    break;
                }
            }
        }
    }
    std::vector<bool> enabled_states;
    enabled_states.reserve(menu_model_.list_entries.size());
    for (const auto& entry : menu_model_.list_entries) {
        enabled_states.push_back(entry.enabled);
    }
    menu_model_.list_entry_cursor = resolveCursorMemoryDefaultIndex(
        remembered, enabled_states, fallback, cursor_memory_enabled_);
}

void BattleScene::populateItemEntries() {
    menu_model_.list_entries.clear();
    menu_model_.list_entry_cursor = -1;
    menu_model_.list_empty_text = "No battle items available";

    if (!item_catalog_) {
        spdlog::warn("BattleScene: Item catalog 不可用，无法生成物品列表。");
        return;
    }

    const auto& item_stocks = session_.itemStocks();
    if (item_stocks.empty()) {
        return;
    }

    auto items = item_catalog_->listItems();
    std::sort(items.begin(), items.end(), [](const game::data::ItemData* lhs, const game::data::ItemData* rhs) {
        const std::string_view lhs_label = lhs && !lhs->display_name_.empty()
            ? std::string_view{lhs->display_name_}
            : (lhs ? std::string_view{lhs->id_str_} : std::string_view{});
        const std::string_view rhs_label = rhs && !rhs->display_name_.empty()
            ? std::string_view{rhs->display_name_}
            : (rhs ? std::string_view{rhs->id_str_} : std::string_view{});
        if (lhs_label == rhs_label) {
            return (lhs ? lhs->id_str_ : std::string{}) < (rhs ? rhs->id_str_ : std::string{});
        }
        return lhs_label < rhs_label;
    });

    int entry_index = 0;
    for (const auto* item : items) {
        if (!item || item->id_str_.empty() || !item->battle_use_) {
            continue;
        }

        const auto stock_it = item_stocks.find(item->id_);
        if (stock_it == item_stocks.end() || stock_it->second <= 0) {
            continue;
        }

        const std::string_view label = item->display_name_.empty()
            ? std::string_view{item->id_str_}
            : std::string_view{item->display_name_};
        menu_model_.list_entries.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = item->id_str_,
            .label = makeRmlString(label),
            .sublabel = itemSubtitle(stock_it->second, *item->battle_use_),
            .enabled = isItemEntryEnabled(stock_it->second, *item->battle_use_)
        });
    }

    const int fallback = firstEnabledListEntryIndex();
    int remembered = -1;
    if (cursor_memory_enabled_) {
        if (const auto* actor = currentActor()) {
            if (const auto it = last_item_id_per_actor_.find(actor->id); it != last_item_id_per_actor_.end()) {
                for (const auto& entry : menu_model_.list_entries) {
                    if (entry.entry_id == it->second) {
                        remembered = entry.entry_index;
                        break;
                    }
                }
            }
        }
    }
    std::vector<bool> enabled_states;
    enabled_states.reserve(menu_model_.list_entries.size());
    for (const auto& entry : menu_model_.list_entries) {
        enabled_states.push_back(entry.enabled);
    }
    menu_model_.list_entry_cursor = resolveCursorMemoryDefaultIndex(
        remembered, enabled_states, fallback, cursor_memory_enabled_);
}

const BattleScene::ListEntryViewModel* BattleScene::findListEntry(int entry_index) const {
    const auto it = std::find_if(
        menu_model_.list_entries.begin(),
        menu_model_.list_entries.end(),
        [entry_index](const ListEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == menu_model_.list_entries.end() ? nullptr : &*it;
}

bool BattleScene::isSkillEntryEnabled(const game::battle::BattleUnit& actor,
                                      const game::data::SkillData& skill) const {
    return actor.mp >= skill.mp_cost_ && skill.scope_ != game::data::Scope::None;
}

Rml::String BattleScene::skillSubtitle(const game::battle::BattleUnit& actor,
                                       const game::data::SkillData& skill) const {
    std::string subtitle = "MP " + std::to_string(skill.mp_cost_);
    if (actor.mp < skill.mp_cost_) {
        subtitle += " / Low MP";
    }
    return subtitle;
}

const game::data::ItemData* BattleScene::findBattleItemByEntryId(std::string_view entry_id,
                                                                 int* out_stock_count) const {
    if (out_stock_count) {
        *out_stock_count = 0;
    }
    if (!item_catalog_ || entry_id.empty()) {
        return nullptr;
    }

    const entt::id_type item_id = game::data::RpgCatalog::hashId(entry_id);
    if (out_stock_count) {
        if (const auto stock_it = session_.itemStocks().find(item_id); stock_it != session_.itemStocks().end()) {
            *out_stock_count = stock_it->second;
        }
    }
    return item_catalog_->findItem(item_id);
}

bool BattleScene::isItemEntryEnabled(int stock_count, const game::data::BattleItemUseConfig& use) const {
    return stock_count >= std::max(1, use.consume) && use.scope != game::data::Scope::None;
}

Rml::String BattleScene::itemSubtitle(int stock_count, const game::data::BattleItemUseConfig& use) const {
    std::string subtitle = "x" + std::to_string(std::max(0, stock_count));
    if (use.consume > 1) {
        subtitle += " / Use " + std::to_string(use.consume);
    }
    if (stock_count < std::max(1, use.consume)) {
        subtitle += " / Low Stock";
    }
    return subtitle;
}

bool BattleScene::requiresTargetSelection(game::data::Scope scope) const {
    return scope == game::data::Scope::OneEnemy || scope == game::data::Scope::OneAlly;
}

int BattleScene::firstEnabledListEntryIndex() const {
    for (const auto& entry : menu_model_.list_entries) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return menu_model_.list_entries.empty() ? -1 : menu_model_.list_entries.front().entry_index;
}

void BattleScene::populateTargetEntries(game::data::Scope scope, const game::battle::BattleUnit& actor) {
    menu_model_.target_entries.clear();
    menu_model_.target_entry_cursor = -1;
    menu_model_.target_empty_text = "No valid targets";

    int entry_index = 0;
    for (const auto& unit : session_.units()) {
        const bool matches_scope = (scope == game::data::Scope::OneEnemy && unit.side != actor.side) ||
            (scope == game::data::Scope::OneAlly && unit.side == actor.side);
        if (!matches_scope) {
            continue;
        }

        menu_model_.target_entries.push_back(TargetEntryViewModel{
            .entry_index = entry_index++,
            .unit_id = static_cast<int>(unit.id),
            .label = targetLabel(unit),
            .sublabel = targetSublabel(unit),
            .enabled = unit.isAlive(),
            .is_ally = unit.side == actor.side,
            .is_dead = !unit.isAlive()
        });
    }

    const int fallback = firstEnabledTargetEntryIndex();
    int remembered = -1;
    if (cursor_memory_enabled_) {
        if (const auto it = last_target_unit_id_per_actor_.find(actor.id); it != last_target_unit_id_per_actor_.end()) {
            for (const auto& entry : menu_model_.target_entries) {
                if (static_cast<game::battle::BattleUnitId>(entry.unit_id) == it->second) {
                    remembered = entry.entry_index;
                    break;
                }
            }
        }
    }
    std::vector<bool> enabled_states;
    enabled_states.reserve(menu_model_.target_entries.size());
    for (const auto& entry : menu_model_.target_entries) {
        enabled_states.push_back(entry.enabled);
    }
    menu_model_.target_entry_cursor = resolveCursorMemoryDefaultIndex(
        remembered, enabled_states, fallback, cursor_memory_enabled_);
}

const BattleScene::TargetEntryViewModel* BattleScene::findTargetEntry(int entry_index) const {
    const auto it = std::find_if(
        menu_model_.target_entries.begin(),
        menu_model_.target_entries.end(),
        [entry_index](const TargetEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == menu_model_.target_entries.end() ? nullptr : &*it;
}

int BattleScene::firstEnabledTargetEntryIndex() const {
    for (const auto& entry : menu_model_.target_entries) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return menu_model_.target_entries.empty() ? -1 : menu_model_.target_entries.front().entry_index;
}

Rml::String BattleScene::targetLabel(const game::battle::BattleUnit& unit) const {
    return unit.name;
}

Rml::String BattleScene::targetSublabel(const game::battle::BattleUnit& unit) const {
    if (!unit.isAlive()) {
        return "(KO)";
    }
    return std::to_string(unit.hp) + "/" + std::to_string(unit.max_hp);
}

BattleScene::MenuState BattleScene::menuStateForActionDraftSource() const {
    switch (action_draft_.pending_type) {
        case game::battle::BattleActionType::Skill:
            return MenuState::SkillList;
        case game::battle::BattleActionType::Item:
            return MenuState::ItemList;
        case game::battle::BattleActionType::Attack:
        case game::battle::BattleActionType::Guard:
        case game::battle::BattleActionType::Escape:
        case game::battle::BattleActionType::EndTurn:
            return MenuState::ActorCommand;
    }
    return MenuState::ActorCommand;
}

void BattleScene::setMenuHint(std::string_view text) {
    menu_model_.setHint(text, document_controller_);
}

void BattleScene::continueDraftAfterScopeSelected(game::data::Scope scope, const game::battle::BattleUnit& actor) {
    action_draft_.requires_target_selection = requiresTargetSelection(scope);
    action_draft_.selected_target_id.reset();

    switch (scope) {
        case game::data::Scope::OneEnemy:
        case game::data::Scope::OneAlly:
            populateTargetEntries(scope, actor);
            setMenuState(MenuState::TargetSelect);
            return;
        case game::data::Scope::Self:
        case game::data::Scope::AllEnemies:
        case game::data::Scope::AllAllies:
            (void)submitDraftAction();
            return;
        case game::data::Scope::None:
            setMenuHint("Action cannot be used.");
            return;
    }
}

bool BattleScene::shouldOpenPartyCommand() const {
    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return false;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id) {
        return false;
    }

    const auto* actor = session_.findUnit(*actor_id);
    if (!actor || actor->side != game::battle::BattleSide::Player || !actor->isAlive()) {
        return false;
    }

    return party_command_accepted_round_ != session_.roundIndex();
}

const BattleScene::CommandViewModel* BattleScene::findPartyCommand(int entry_index) const {
    const auto it = std::find_if(
        menu_model_.party_commands.begin(),
        menu_model_.party_commands.end(),
        [entry_index](const CommandViewModel& command) {
            return command.entry_index == entry_index;
        });
    return it == menu_model_.party_commands.end() ? nullptr : &*it;
}

const BattleScene::CommandViewModel* BattleScene::findActorCommand(int entry_index) const {
    const auto it = std::find_if(
        menu_model_.actor_commands.begin(),
        menu_model_.actor_commands.end(),
        [entry_index](const CommandViewModel& command) {
            return command.entry_index == entry_index;
        });
    return it == menu_model_.actor_commands.end() ? nullptr : &*it;
}

int BattleScene::firstEnabledPartyCommandIndex() const {
    for (const auto& command : menu_model_.party_commands) {
        if (command.enabled) {
            return command.entry_index;
        }
    }
    return menu_model_.party_commands.empty() ? -1 : menu_model_.party_commands.front().entry_index;
}

int BattleScene::firstEnabledActorCommandIndex() const {
    for (const auto& command : menu_model_.actor_commands) {
        if (command.enabled) {
            return command.entry_index;
        }
    }
    return menu_model_.actor_commands.empty() ? -1 : menu_model_.actor_commands.front().entry_index;
}

void BattleScene::handlePartyCommand(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(menu_model_.party_commands.size())) {
        return;
    }

    const auto* command = findPartyCommand(entry_index);
    if (!command) {
        return;
    }

    menu_model_.party_command_cursor = command->entry_index;
    menu_model_.focus_dirty = true;
    if (!command->enabled) {
        return;
    }

    switch (static_cast<PartyCommandId>(command->command_id)) {
        case PartyCommandId::Fight:
            party_command_accepted_round_ = session_.roundIndex();
            actor_command_entered_via_fight_this_step_ = true;
            setMenuState(MenuState::ActorCommand);
            return;
        case PartyCommandId::Escape:
            actor_command_entered_via_fight_this_step_ = false;
            queueEscapeAction();
            return;
    }
}

void BattleScene::handleActorCommand(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(menu_model_.actor_commands.size())) {
        return;
    }

    const auto* command = findActorCommand(entry_index);
    if (!command) {
        return;
    }

    menu_model_.actor_command_cursor = command->entry_index;
    menu_model_.focus_dirty = true;
    if (!command->enabled) {
        return;
    }

    actor_command_entered_via_fight_this_step_ = false;

    if (const auto* actor = currentActor()) {
        last_actor_command_index_per_actor_[actor->id] = entry_index;
    }

    switch (static_cast<ActorCommandId>(command->command_id)) {
        case ActorCommandId::Attack:
            queueAttackAction();
            return;
        case ActorCommandId::Skill:
            queueSkillAction();
            return;
        case ActorCommandId::Guard:
            queueGuardAction();
            return;
        case ActorCommandId::Item:
            queueItemAction();
            return;
    }
}

void BattleScene::handleListEntry(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(menu_model_.list_entries.size())) {
        return;
    }

    const auto* entry = findListEntry(entry_index);
    if (!entry) {
        return;
    }

    menu_model_.list_entry_cursor = entry->entry_index;
    menu_model_.focus_dirty = true;
    if (!entry->enabled) {
        return;
    }

    actor_command_entered_via_fight_this_step_ = false;
    if (menu_model_.state == MenuState::SkillList) {
        handleSkillEntry(*entry);
    } else if (menu_model_.state == MenuState::ItemList) {
        handleItemEntry(*entry);
    }
}

void BattleScene::handleSkillEntry(const ListEntryViewModel& entry) {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor || !rpg_catalog_) {
        return;
    }

    const auto* skill = rpg_catalog_->findSkill(entry.entry_id);
    if (!skill || !isSkillEntryEnabled(*actor, *skill)) {
        return;
    }

    last_skill_id_per_actor_[actor->id] = skill->id_;
    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Skill,
        .selected_skill_id = skill->id_,
        .selected_item_id = std::nullopt,
        .selected_target_id = std::nullopt
    };
    continueDraftAfterScopeSelected(skill->scope_, *actor);
}

void BattleScene::handleItemEntry(const ListEntryViewModel& entry) {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    int stock_count = 0;
    const auto* item = findBattleItemByEntryId(entry.entry_id, &stock_count);
    if (!item || !item->battle_use_ || !isItemEntryEnabled(stock_count, *item->battle_use_)) {
        return;
    }

    last_item_id_per_actor_[actor->id] = item->id_str_;
    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Item,
        .selected_skill_id = std::nullopt,
        .selected_item_id = item->id_str_,
        .selected_target_id = std::nullopt
    };
    continueDraftAfterScopeSelected(item->battle_use_->scope, *actor);
}

void BattleScene::handleTargetEntry(int entry_index) {
    if (!isWaitingForActionInput()) {
        return;
    }

    const auto* entry = findTargetEntry(entry_index);
    if (!entry) {
        return;
    }

    menu_model_.target_entry_cursor = entry->entry_index;
    menu_model_.focus_dirty = true;
    if (!entry->enabled) {
        return;
    }

    actor_command_entered_via_fight_this_step_ = false;
    if (const auto* actor = currentActor()) {
        last_target_unit_id_per_actor_[actor->id] =
            static_cast<game::battle::BattleUnitId>(entry->unit_id);
    }
    action_draft_.selected_target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
    (void)submitDraftAction();
}

void BattleScene::handleStateIconHoverEnter(int unit_id, int entry_index) {
    const auto icon_it = std::find_if(
        party_state_icons_.begin(),
        party_state_icons_.end(),
        [unit_id, entry_index](const StateIconViewModel& icon) {
            return icon.unit_id == unit_id && icon.entry_index == entry_index;
        });
    if (icon_it == party_state_icons_.end()) {
        hideStateTooltip();
        return;
    }

    const StateTooltipViewModel next_tooltip{
        .active_unit_id = icon_it->unit_id,
        .title = icon_it->display_name,
        .turns = icon_it->turns_text + " Turns",
        .description = icon_it->description,
        .visible = true
    };
    if (state_tooltip_ != next_tooltip || state_tooltip_entry_index_ != entry_index) {
        state_tooltip_ = next_tooltip;
        state_tooltip_entry_index_ = entry_index;
        document_controller_.markDirty("state_tooltip");
    }
}

void BattleScene::handleStateIconHoverExit(int unit_id, int entry_index) {
    if (!state_tooltip_.visible ||
        state_tooltip_.active_unit_id != unit_id ||
        state_tooltip_entry_index_ != entry_index) {
        return;
    }

    hideStateTooltip();
}

bool BattleScene::submitDraftAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        setMenuHint("Action is no longer available.");
        return false;
    }

    actor_command_entered_via_fight_this_step_ = false;

    if (action_draft_.requires_target_selection && !action_draft_.selected_target_id) {
        setMenuHint("Choose a target.");
        return false;
    }

    switch (action_draft_.pending_type) {
        case game::battle::BattleActionType::Attack:
            if (!action_draft_.selected_target_id) {
                setMenuHint("Choose a target.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Attack,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id
            });
            return true;
        case game::battle::BattleActionType::Skill:
            if (!action_draft_.selected_skill_id) {
                setMenuHint("Action is no longer available.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Skill,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id,
                .skill_id = *action_draft_.selected_skill_id
            });
            return true;
        case game::battle::BattleActionType::Item:
            if (!action_draft_.selected_item_id) {
                setMenuHint("Action is no longer available.");
                return false;
            }
            submitAction(game::battle::BattleAction{
                .type = game::battle::BattleActionType::Item,
                .actor_id = actor_id,
                .target_id = action_draft_.selected_target_id,
                .item_id = *action_draft_.selected_item_id
            });
            return true;
        case game::battle::BattleActionType::Guard:
        case game::battle::BattleActionType::Escape:
        case game::battle::BattleActionType::EndTurn:
            setMenuHint("Action is no longer available.");
            return false;
    }

    return false;
}

void BattleScene::submitAction(game::battle::BattleAction action) {
    pending_action_ = std::move(action);
    leaveInputMenu();
    flow_controller_.beginExecutingAction();
}

bool BattleScene::isWaitingForActionInput() const {
    return !end_requested_ &&
        flow_controller_.isWaitingForInput() &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        session_.currentActorId().has_value();
}

BattleMenuState BattleScene::battleMenuState() const {
    return menu_model_.state;
}

bool BattleScene::moveBattleMenuCursor(const int delta) {
    return moveMenuCursor(delta);
}

bool BattleScene::moveMenuCursor(int delta) {
    if (!isWaitingForActionInput() || delta == 0) {
        return false;
    }

    switch (menu_model_.state) {
        case MenuState::PartyCommand: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(menu_model_.party_commands.size());
            for (const auto& command : menu_model_.party_commands) {
                enabled_entries.push_back(command.enabled);
            }

            if (!moveCursorInEntries(menu_model_.party_command_cursor, static_cast<int>(menu_model_.party_commands.size()), delta, enabled_entries)) {
                return false;
            }
            menu_model_.focus_dirty = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::ActorCommand: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(menu_model_.actor_commands.size());
            for (const auto& command : menu_model_.actor_commands) {
                enabled_entries.push_back(command.enabled);
            }

            if (!moveCursorInEntries(menu_model_.actor_command_cursor, static_cast<int>(menu_model_.actor_commands.size()), delta, enabled_entries)) {
                return false;
            }
            menu_model_.focus_dirty = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::SkillList:
        case MenuState::ItemList: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(menu_model_.list_entries.size());
            for (const auto& entry : menu_model_.list_entries) {
                enabled_entries.push_back(entry.enabled);
            }

            if (!moveCursorInEntries(menu_model_.list_entry_cursor, static_cast<int>(menu_model_.list_entries.size()), delta, enabled_entries)) {
                return false;
            }
            menu_model_.focus_dirty = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::TargetSelect: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(menu_model_.target_entries.size());
            for (const auto& target : menu_model_.target_entries) {
                enabled_entries.push_back(target.enabled);
            }

            if (!moveCursorInEntries(menu_model_.target_entry_cursor, static_cast<int>(menu_model_.target_entries.size()), delta, enabled_entries)) {
                return false;
            }
            menu_model_.focus_dirty = true;
            syncEnemyHpBarHighlight();
            syncMenuFocus();
            return true;
        }
        case MenuState::None:
            return false;
    }
}

bool BattleScene::moveCursorInEntries(int& cursor, int count, int step, const std::vector<bool>& enabled_entries) {
    if (count <= 0 || enabled_entries.empty()) {
        return false;
    }

    const int start = cursor >= 0 && cursor < count ? cursor : 0;
    for (int offset = 1; offset <= count; ++offset) {
        const int raw_candidate = start + step * offset;
        const int candidate = (raw_candidate % count + count) % count;
        if (candidate >= static_cast<int>(enabled_entries.size()) || !enabled_entries[candidate]) {
            continue;
        }

        if (candidate == cursor) {
            return false;
        }

        cursor = candidate;
        return true;
    }

    return false;
}

bool BattleScene::confirmBattleMenu() {
    if (flow_controller_.isVictoryFlow()) {
        victory_flow_controller_.confirm();
        return true;
    }

    if (!isWaitingForActionInput()) {
        return menu_model_.state != MenuState::None;
    }

    switch (menu_model_.state) {
        case MenuState::PartyCommand:
            handlePartyCommand(menu_model_.party_command_cursor);
            return true;
        case MenuState::ActorCommand:
            handleActorCommand(menu_model_.actor_command_cursor);
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            handleListEntry(menu_model_.list_entry_cursor);
            return true;
        case MenuState::TargetSelect:
            handleTargetEntry(menu_model_.target_entry_cursor);
            return true;
        case MenuState::None:
            return false;
    }
}

bool BattleScene::cancelBattleMenu() {
    if (flow_controller_.isVictoryFlow()) {
        return true;
    }

    if (!isWaitingForActionInput()) {
        return menu_model_.state != MenuState::None;
    }

    switch (menu_model_.state) {
        case MenuState::TargetSelect:
            action_draft_.selected_target_id.reset();
            setMenuState(menuStateForActionDraftSource());
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            action_draft_ = {};
            actor_command_entered_via_fight_this_step_ = false;
            setMenuState(MenuState::ActorCommand);
            return true;
        case MenuState::ActorCommand:
            if (actor_command_entered_via_fight_this_step_) {
                actor_command_entered_via_fight_this_step_ = false;
                party_command_accepted_round_.reset();
                setMenuState(MenuState::PartyCommand);
            }
            return true;
        case MenuState::PartyCommand:
            return true;
        case MenuState::None:
            return false;
    }
}

void BattleScene::queueAttackAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Attack,
        .requires_target_selection = true
    };
    continueDraftAfterScopeSelected(game::data::Scope::OneEnemy, *actor);
}

void BattleScene::queueSkillAction() {
    game::battle::BattleUnitId actor_id = 0;
    const auto* actor = prepareActionActor(actor_id);
    if (!actor) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Skill,
        .requires_target_selection = true
    };
    populateSkillEntries(*actor);
    setMenuState(MenuState::SkillList);
}

void BattleScene::queueItemAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    action_draft_ = ActionDraft{
        .pending_type = game::battle::BattleActionType::Item,
        .requires_target_selection = false
    };
    populateItemEntries();
    setMenuState(MenuState::ItemList);
}

void BattleScene::queueGuardAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::Guard,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
}

void BattleScene::queueEscapeAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::Escape,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
}

const game::battle::BattleUnit* BattleScene::prepareActionActor(game::battle::BattleUnitId& out_actor_id) const {
    if (!flow_controller_.isWaitingForInput() || session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        return nullptr;
    }

    const auto actor_id = session_.currentActorId();
    if (!actor_id) {
        return nullptr;
    }

    const auto* actor = session_.findUnit(*actor_id);
    if (!actor) {
        return nullptr;
    }

    out_actor_id = *actor_id;
    return actor;
}

void BattleScene::beginVictoryFlow() {
    leaveInputMenu();
    victory_reward_summary_ = resolveVictoryRewards();
    std::vector<std::string> actor_ids{};
    actor_ids.reserve(session_.units().size());
    for (const auto& unit : session_.units()) {
        if (unit.side == game::battle::BattleSide::Player && unit.source_actor_id) {
            actor_ids.push_back(*unit.source_actor_id);
        }
    }
    const game::domain::PartyExperienceGrantResult experience_preview = rpg_catalog_
        ? game::domain::ActorProgressionService::previewExperience(
              *rpg_catalog_,
              actor_ids,
              presentation_options_.actor_runtime_states,
              presentation_options_.actor_equipment,
              victory_reward_summary_->exp_total)
        : game::domain::PartyExperienceGrantResult{};
    victory_flow_controller_.begin(*victory_reward_summary_, experience_preview);
    victory_continue_focus_dirty_ = false;
    battle_enemy_hp_bar_controller_.setHighlightedTarget(std::nullopt);
    playVictoryAudioCue();
}

game::battle::BattleRewardSummary BattleScene::resolveVictoryRewards() {
    if (!rpg_catalog_) {
        spdlog::warn("BattleScene: RPG catalog 不可用，Victory 奖励摘要为空。");
        return {};
    }

    game::battle::BattleRewardResolver resolver{};
    return resolver.resolve(game::battle::BattleOutcome::Victory, session_.units(), *rpg_catalog_);
}

void BattleScene::finishVictoryFlow() {
    victory_flow_controller_.reset();
}

void BattleScene::updateVictoryFlow(const float delta_time) {
    victory_flow_controller_.update(delta_time);
}

bool BattleScene::victoryFlowFinished() const {
    return victory_flow_controller_.finished();
}

void BattleScene::playVictoryAudioCue() {
    // 占位 Victory ME；后续替换为专用 fanfare 资源。
    static_cast<void>(context_.getAudioPlayer().playSound(game::defs::audio::BATTLE_VICTORY_ID.value()));
}

std::vector<BattlePresentationUnitAnchor> BattleScene::collectBattlePresentationUnitAnchors() const {
    std::vector<BattlePresentationUnitAnchor> anchors;
    const auto view = battle_registry_.view<BattleSpriteComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        const auto* unit = session_.findUnit(sprite.unit_id);
        anchors.push_back(BattlePresentationUnitAnchor{
            .unit_id = sprite.unit_id,
            .side = sprite.side,
            .base_screen_position = sprite.screen_position,
            .alive_after = unit ? unit->isAlive() : false
        });
    }
    return anchors;
}

BattleAnimationTimelineConfig BattleScene::animationConfigForPlan(
    const BattleActionPresentationPlan& plan) const {
    BattleAnimationTimelineConfig config{};
    config.motion_style = plan.motion_style;
    config.actor_start_offset = plan.actor_start_offset;
    config.duration_seconds = plan.duration_seconds;
    config.impact_time_seconds = plan.impact_time_seconds;
    config.hit_feedback_duration_seconds = plan.recovery_time_seconds;
    scaleAnimationTimeline(config, battle_animation_speed_);
    return config;
}

BattleActionPresentationPlan BattleScene::presentationPlanForResult(
    const game::battle::BattleActionResult& result,
    const std::vector<BattlePresentationUnitAnchor>& unit_anchors) const {
    const auto* skill = result.action_type == game::battle::BattleActionType::Skill && rpg_catalog_
        ? rpg_catalog_->findSkill(result.skill_id)
        : nullptr;
    const auto* default_hit_skill = rpg_catalog_ ? rpg_catalog_->findSkill(BASIC_ATTACK_SKILL_ID) : nullptr;
    return buildBattleActionPresentationPlan(BattleActionPresentationPlanRequest{
        .result = &result,
        .skill = skill,
        .default_attack_skill = default_hit_skill,
        .unit_anchors = &unit_anchors,
        .actor_start_offset = actionStartOffsetFor(result.actor_id)
    });
}

glm::vec2 BattleScene::actionStartOffsetFor(const game::battle::BattleUnitId actor_id) const {
    if (!command_focus_actor_id_ || *command_focus_actor_id_ != actor_id) {
        return glm::vec2{0.0F, 0.0F};
    }

    const auto* unit = session_.findUnit(actor_id);
    if (!unit || unit->side != game::battle::BattleSide::Player || !unit->isAlive()) {
        return glm::vec2{0.0F, 0.0F};
    }

    const float t = std::clamp(command_focus_elapsed_seconds_ / COMMAND_FOCUS_EASE_SECONDS, 0.0F, 1.0F);
    const float eased = 1.0F - (1.0F - t) * (1.0F - t);
    return COMMAND_FOCUS_PLAYER_OFFSET * eased;
}

void BattleScene::schedulePresentationPlanEvents(
    const BattleActionPresentationPlan& plan,
    const game::battle::BattleActionResult& result) {
    scheduled_presentation_events_.clear();
    for (const auto& marker : plan.markers) {
        switch (marker.type) {
            case BattlePresentationMarkerType::TargetVfx:
                schedulePresentationEvent(marker.vfx_command, marker.time_seconds);
                break;
            case BattlePresentationMarkerType::TargetSfx:
                schedulePresentationEvent(marker.sound_event, marker.time_seconds);
                break;
            case BattlePresentationMarkerType::EnemyHpReveal:
                scheduleEnemyHpRevealEvent(result, marker.time_seconds);
                break;
        }
    }
}

void BattleScene::schedulePresentationEvent(engine::vfx::PlayVfxCommand command, const float fire_time_seconds) {
    scheduled_presentation_events_.push_back(ScheduledPresentationEvent{
        .payload = std::move(command),
        .remaining_seconds = std::max(0.0F, fire_time_seconds),
    });
}

void BattleScene::schedulePresentationEvent(engine::utils::PlaySoundEvent event, const float fire_time_seconds) {
    scheduled_presentation_events_.push_back(ScheduledPresentationEvent{
        .payload = std::move(event),
        .remaining_seconds = std::max(0.0F, fire_time_seconds),
    });
}

void BattleScene::scheduleEnemyHpRevealEvent(game::battle::BattleActionResult result, const float fire_time_seconds) {
    scheduled_presentation_events_.push_back(ScheduledPresentationEvent{
        .payload = std::move(result),
        .remaining_seconds = std::max(0.0F, fire_time_seconds),
    });
}

void BattleScene::updateScheduledPresentationEvents(const float delta_time) {
    if (scheduled_presentation_events_.empty()) {
        return;
    }

    const float delta = std::clamp(delta_time, 0.0F, 0.25F);
    for (auto it = scheduled_presentation_events_.begin(); it != scheduled_presentation_events_.end();) {
        it->remaining_seconds -= delta;
        if (it->remaining_seconds > 0.0F) {
            ++it;
            continue;
        }

        std::visit([this](auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, game::battle::BattleActionResult>) {
                battle_enemy_hp_bar_controller_.applyStagedSnapshotAndReveal(payload);
                return;
            }
            auto event = Payload{payload};
            context_.getDispatcher().trigger<Payload>(std::move(event));
        }, it->payload);
        it = scheduled_presentation_events_.erase(it);
    }
}

void BattleScene::updateCommandFocus(const float delta_time) {
    std::optional<game::battle::BattleUnitId> next_actor_id{};
    if (flow_controller_.isWaitingForInput() && !battle_animation_director_.active()) {
        if (const auto current_actor_id = session_.currentActorId()) {
            if (const auto* unit = session_.findUnit(*current_actor_id);
                unit && unit->side == game::battle::BattleSide::Player && unit->isAlive()) {
                next_actor_id = *current_actor_id;
            }
        }
    }

    if (next_actor_id != command_focus_actor_id_) {
        command_focus_actor_id_ = next_actor_id;
        command_focus_elapsed_seconds_ = 0.0F;
        return;
    }

    if (command_focus_actor_id_) {
        command_focus_elapsed_seconds_ = std::min(command_focus_elapsed_seconds_ + delta_time,
                                                  COMMAND_FOCUS_EASE_SECONDS);
    } else {
        command_focus_elapsed_seconds_ = 0.0F;
    }
}

std::optional<BattleAnimationPose> BattleScene::commandFocusPoseFor(
    const game::battle::BattleUnitId unit_id,
    const game::battle::BattleSide side) const {
    if (!flow_controller_.isWaitingForInput() || battle_animation_director_.active() ||
        side != game::battle::BattleSide::Player) {
        return std::nullopt;
    }

    if (!command_focus_actor_id_ || *command_focus_actor_id_ != unit_id) {
        return std::nullopt;
    }

    const auto* unit = session_.findUnit(unit_id);
    if (!unit || !unit->isAlive()) {
        return std::nullopt;
    }

    BattleAnimationPose pose{};
    const float t = std::clamp(command_focus_elapsed_seconds_ / COMMAND_FOCUS_EASE_SECONDS, 0.0F, 1.0F);
    const float eased = 1.0F - (1.0F - t) * (1.0F - t);
    pose.offset = COMMAND_FOCUS_PLAYER_OFFSET * eased;
    pose.color_multiplier = engine::utils::FColor{1.06F, 1.04F, 0.88F, 1.0F};
    return pose;
}

std::optional<BattleAnimationPose> BattleScene::presentationPoseFor(
    const game::battle::BattleUnitId unit_id,
    const game::battle::BattleSide side) const {
    if (const auto pose = battle_animation_director_.poseFor(unit_id)) {
        return pose;
    }
    if (flow_controller_.isVictoryFlow() && side == game::battle::BattleSide::Player) {
        const auto* unit = session_.findUnit(unit_id);
        if (unit && unit->isAlive()) {
            const BattleVictoryFlowSnapshot victory_snapshot = victory_flow_controller_.snapshot();
            const float bob = std::sin(victory_snapshot.elapsed_seconds * VICTORY_POSE_BOB_TAU * VICTORY_POSE_BOB_RATE) *
                VICTORY_POSE_BOB_PIXELS;
            BattleAnimationPose pose{};
            pose.offset = VICTORY_POSE_PLAYER_BASE_OFFSET + glm::vec2{0.0F, bob};
            pose.color_multiplier = engine::utils::FColor{1.10F, 1.08F, 0.90F, 1.0F};
            return pose;
        }
    }
    return commandFocusPoseFor(unit_id, side);
}

bool BattleScene::initPresentation() {
    auto& resource_manager = context_.getResourceManager();
    if (auto* resource_ptr = battle_registry_.ctx().find<engine::resource::ResourceManager*>()) {
        *resource_ptr = &resource_manager;
    } else {
        battle_registry_.ctx().emplace<engine::resource::ResourceManager*>(&resource_manager);
    }

    if (!presentation_options_.battle_background_id.empty()) {
        battle_background_.load(presentation_options_.battle_background_id, resource_manager);
    }

    if (!resource_manager.findLoadedTexture(engine::resource::defaults::CIRCLE_TEXTURE_ID)) {
        resource_manager.loadTexture(engine::resource::defaults::CIRCLE_TEXTURE_ID,
                                     engine::resource::defaults::CIRCLE_TEXTURE_PATH);
    }
    resource_manager.loadFont(engine::resource::defaults::UI_DEFAULT_FONT_ID,
                              DAMAGE_POPUP_FONT_SIZE_PX,
                              engine::resource::defaults::UI_DEFAULT_FONT_PATH);

    if (!blueprint_manager_) {
        spdlog::warn("BattleScene: 缺少 BlueprintManager，战斗角色表现将不绘制。");
        return true;
    }

    std::size_t player_count = 0;
    std::size_t enemy_count = 0;
    for (const auto& unit : session_.units()) {
        if (unit.side == game::battle::BattleSide::Player) {
            ++player_count;
        } else {
            ++enemy_count;
        }
    }

    std::size_t player_index = 0;
    std::size_t enemy_index = 0;
    for (const auto& unit : session_.units()) {
        std::string blueprint_id;
        std::string idle_animation = unit.side == game::battle::BattleSide::Player ? "idle_left" : "idle_right";
        float scale = 2.0F;
        glm::vec2 shadow_offset{0.0F, 0.0F};
        std::optional<AppearanceSnapshot> appearance_snapshot{};

        const auto seed_it = std::find_if(
            presentation_options_.sprite_seeds.begin(),
            presentation_options_.sprite_seeds.end(),
            [&unit](const BattleSpriteSeed& seed) {
                return seed.unit_id == unit.id;
            });
        if (seed_it != presentation_options_.sprite_seeds.end()) {
            appearance_snapshot = seed_it->appearance;
        }

        if (unit.side == game::battle::BattleSide::Player) {
            if (rpg_catalog_ && unit.source_actor_id) {
                if (const auto* actor = rpg_catalog_->findActor(*unit.source_actor_id)) {
                    if (actor->battle_visual_.valid()) {
                        blueprint_id = actor->battle_visual_.sprite_blueprint_id_;
                        idle_animation = actor->battle_visual_.idle_animation_;
                        scale = actor->battle_visual_.sprite_scale_;
                        shadow_offset = actor->battle_visual_.shadow_offset_;
                    } else {
                        blueprint_id = actor->map_actor_id_;
                    }
                }
            }
            if (blueprint_id.empty() && unit.source_actor_id) {
                constexpr std::string_view prefix = "actor.";
                blueprint_id = unit.source_actor_id->starts_with(prefix)
                    ? unit.source_actor_id->substr(prefix.size())
                    : *unit.source_actor_id;
            }
        } else if (rpg_catalog_ && unit.source_enemy_id) {
            if (const auto* enemy = rpg_catalog_->findEnemy(*unit.source_enemy_id)) {
                if (enemy->battle_visual_.valid()) {
                    blueprint_id = enemy->battle_visual_.sprite_blueprint_id_;
                    idle_animation = enemy->battle_visual_.idle_animation_;
                    scale = enemy->battle_visual_.sprite_scale_;
                    shadow_offset = enemy->battle_visual_.shadow_offset_;
                } else {
                    spdlog::warn("BattleScene: enemy '{}' 缺少 battle_visual，已跳过战斗精灵。", enemy->id_);
                }
            }
        }

        if (blueprint_id.empty()) {
            spdlog::warn("BattleScene: unit '{}' 无法解析战斗精灵蓝图。", unit.name);
            continue;
        }

        const entt::id_type blueprint_hash = hashString(blueprint_id);
        if (!blueprint_manager_->hasActorBlueprint(blueprint_hash)) {
            spdlog::warn("BattleScene: 战斗精灵蓝图 '{}' 不存在。", blueprint_id);
            continue;
        }

        const auto& blueprint = blueprint_manager_->getActorBlueprint(blueprint_hash);
        if (!resource_manager.findLoadedTexture(blueprint.sprite_.id_)) {
            resource_manager.loadTexture(blueprint.sprite_.id_, blueprint.sprite_.path_);
        }
        for (const auto& [_, animation] : blueprint.animations_) {
            if (!resource_manager.findLoadedTexture(animation.texture_id_)) {
                resource_manager.loadTexture(animation.texture_id_, animation.texture_path_);
            }
        }

        const std::size_t side_index = unit.side == game::battle::BattleSide::Player ? player_index++ : enemy_index++;
        const std::size_t side_count = unit.side == game::battle::BattleSide::Player ? player_count : enemy_count;
        const BattleFormationSlot formation_slot = battleFormationSlot(unit.side, side_index, side_count, scale);

        auto animations = toRuntimeAnimations(blueprint.animations_);
        entt::id_type idle_animation_id = hashString(idle_animation);
        if (!animations.contains(idle_animation_id)) {
            spdlog::warn("BattleScene: 蓝图 '{}' 缺少动画 '{}'，回退到首个动画。", blueprint_id, idle_animation);
            if (animations.empty()) {
                continue;
            }
            idle_animation_id = animations.begin()->first;
        }

        const entt::entity entity = battle_registry_.create();
        battle_registry_.emplace<engine::component::TransformComponent>(entity);
        auto& sprite = battle_registry_.emplace<engine::component::SpriteComponent>(
            entity,
            engine::component::Sprite{blueprint.sprite_.id_, blueprint.sprite_.src_rect_, blueprint.sprite_.flip_horizontal_},
            blueprint.sprite_.dst_size_,
            blueprint.sprite_.pivot_);
        auto& animation = battle_registry_.emplace<engine::component::AnimationComponent>(
            entity,
            std::move(animations),
            idle_animation_id);
        applyAnimationFrame(animation, sprite);
        const entt::entity shadow_entity = battle_registry_.create();
        battle_registry_.emplace<BattleShadowComponent>(shadow_entity, unit.id, false);
        battle_registry_.emplace<engine::component::TransformComponent>(shadow_entity);
        battle_registry_.emplace<engine::component::SpriteComponent>(
            shadow_entity,
            engine::component::Sprite{
                engine::resource::defaults::CIRCLE_TEXTURE_ID,
                engine::utils::Rect{0.0F, 0.0F, 960.0F, 960.0F}},
            glm::vec2{1.0F, 1.0F},
            glm::vec2{0.5F, 0.5F});
        battle_registry_.emplace<engine::component::RenderComponent>(
            shadow_entity,
            BATTLE_RENDER_LAYER,
            formation_slot.depth + BATTLE_SHADOW_DEPTH_OFFSET,
            engine::utils::FColor{0.03F, 0.05F, 0.08F, BATTLE_SHADOW_ALPHA});

        const entt::entity target_shadow_entity = battle_registry_.create();
        battle_registry_.emplace<BattleShadowComponent>(target_shadow_entity, unit.id, true);
        battle_registry_.emplace<engine::component::TransformComponent>(target_shadow_entity);
        battle_registry_.emplace<engine::component::SpriteComponent>(
            target_shadow_entity,
            engine::component::Sprite{
                engine::resource::defaults::CIRCLE_TEXTURE_ID,
                engine::utils::Rect{0.0F, 0.0F, 960.0F, 960.0F}},
            glm::vec2{1.0F, 1.0F},
            glm::vec2{0.5F, 0.5F});
        battle_registry_.emplace<engine::component::RenderComponent>(
            target_shadow_entity,
            BATTLE_RENDER_LAYER,
            formation_slot.depth + BATTLE_TARGET_SHADOW_DEPTH_OFFSET,
            engine::utils::FColor{0.0F, 0.0F, 0.0F, 0.0F});

        battle_registry_.emplace<engine::component::RenderComponent>(entity, BATTLE_RENDER_LAYER, formation_slot.depth);
        auto& battle_sprite = battle_registry_.emplace<BattleSpriteComponent>(
            entity,
            unit.id,
            unit.side,
            formation_slot.screen_position,
            formation_slot.scale,
            formation_slot.depth,
            formation_slot.shadow_size,
            shadow_offset);
        battle_sprite.shadow_entity = shadow_entity;
        battle_sprite.target_shadow_entity = target_shadow_entity;

        if (appearance_snapshot && appearance_snapshot->valid && appearance_catalog_) {
            game::component::AppearanceComponent appearance{};
            appearance.profile_id_ = appearance_snapshot->profile_id;
            appearance.gender_ = appearance_snapshot->gender.empty() ? std::string{"male"} : appearance_snapshot->gender;
            appearance.slot_variants_ = appearance_snapshot->slot_variants;
            appearance.dirty_ = true;
            battle_registry_.emplace<game::component::AppearanceComponent>(entity, std::move(appearance));
            battle_registry_.emplace<engine::component::LayeredSpriteComponent>(entity);
            game::system::AppearanceLayerCacheBuilder::rebuild(
                battle_registry_,
                entity,
                *appearance_catalog_,
                &resource_manager);
        }
    }

    syncPresentationTransforms();
    syncPresentationShadows();
    refreshPresentation();
    battle_enemy_hp_bar_controller_.syncFromSnapshot(session_.snapshot());
    return true;
}

void BattleScene::updatePresentation(float delta_time) {
    auto view = battle_registry_.view<BattleSpriteComponent,
                                      engine::component::AnimationComponent,
                                      engine::component::SpriteComponent>();
    for (auto entity : view) {
        const auto& battle_sprite = view.get<BattleSpriteComponent>(entity);
        const auto* unit = session_.findUnit(battle_sprite.unit_id);
        if (!unit || !unit->isAlive()) {
            continue;
        }

        auto& animation = view.get<engine::component::AnimationComponent>(entity);
        auto& sprite = view.get<engine::component::SpriteComponent>(entity);
        advanceAnimation(animation, sprite, delta_time);
    }
}

void BattleScene::refreshPresentation() {
    std::optional<game::battle::BattleUnitId> target_id{};
    if (menu_model_.state == MenuState::TargetSelect) {
        if (const auto* entry = findTargetEntry(menu_model_.target_entry_cursor); entry && entry->enabled) {
            target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
        }
    } else {
        target_id = action_draft_.selected_target_id;
    }

    const auto current_actor_id = session_.currentActorId();
    auto view = battle_registry_.view<BattleSpriteComponent, engine::component::RenderComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        auto& render = view.get<engine::component::RenderComponent>(entity);

        const auto* unit = session_.findUnit(sprite.unit_id);
        render.color_ = engine::utils::FColor::white();
        if (!unit || !unit->isAlive()) {
            render.color_ = engine::utils::FColor{0.45F, 0.48F, 0.55F, 0.65F};
        } else if (target_id && *target_id == sprite.unit_id) {
            render.color_ = engine::utils::FColor{1.0F, 0.82F, 0.42F, 1.0F};
        } else if (current_actor_id && *current_actor_id == sprite.unit_id) {
            render.color_ = engine::utils::FColor{1.0F, 0.95F, 0.72F, 1.0F};
        }
        if (const auto pose = presentationPoseFor(sprite.unit_id, sprite.side)) {
            render.color_ = multiplyColor(render.color_, pose->color_multiplier);
            render.depth_ = sprite.depth + pose->offset.y;
        } else {
            render.depth_ = sprite.depth;
        }
    }
}

void BattleScene::syncPresentationTransforms() {
    const auto& camera = context_.getCamera();
    auto view = battle_registry_.view<BattleSpriteComponent, engine::component::TransformComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        glm::vec2 screen_position = sprite.screen_position;
        float scale_multiplier = 1.0F;
        float rotation = 0.0F;
        if (const auto pose = presentationPoseFor(sprite.unit_id, sprite.side)) {
            screen_position += pose->offset;
            scale_multiplier = pose->scale_multiplier;
            rotation = pose->rotation_radians;
        }

        const glm::vec2 position = camera.screenToWorld(screen_position);
        transform.position_ = position;
        transform.previous_position_ = position;
        transform.scale_ = glm::vec2{sprite.scale * scale_multiplier, sprite.scale * scale_multiplier};
        transform.rotation_ = rotation;
    }
}

void BattleScene::syncPresentationShadows() {
    const auto& camera = context_.getCamera();
    std::optional<game::battle::BattleUnitId> target_id{};
    if (menu_model_.state == MenuState::TargetSelect) {
        if (const auto* entry = findTargetEntry(menu_model_.target_entry_cursor); entry && entry->enabled) {
            target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
        }
    }

    auto view = battle_registry_.view<BattleSpriteComponent, engine::component::SpriteComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        const auto& visual = view.get<engine::component::SpriteComponent>(entity);
        const auto* unit = session_.findUnit(sprite.unit_id);

        glm::vec2 screen_position = battleShadowScreenPosition(sprite, visual);
        float pose_depth_offset = 0.0F;
        if (const auto pose = presentationPoseFor(sprite.unit_id, sprite.side)) {
            screen_position += pose->offset;
            pose_depth_offset = pose->offset.y;
        }
        const glm::vec2 world_position = camera.screenToWorld(screen_position);

        if (auto* shadow_transform = battle_registry_.try_get<engine::component::TransformComponent>(sprite.shadow_entity);
            shadow_transform) {
            shadow_transform->position_ = world_position;
            shadow_transform->previous_position_ = world_position;
            shadow_transform->scale_ = glm::vec2{sprite.shadow_size.x, sprite.shadow_size.y};
            shadow_transform->rotation_ = 0.0F;
        }
        if (auto* shadow_render = battle_registry_.try_get<engine::component::RenderComponent>(sprite.shadow_entity);
            shadow_render) {
            shadow_render->color_ = unit && unit->isAlive()
                ? engine::utils::FColor{0.03F, 0.05F, 0.08F, BATTLE_SHADOW_ALPHA}
                : engine::utils::FColor{0.0F, 0.0F, 0.0F, 0.0F};
            shadow_render->depth_ = sprite.depth + pose_depth_offset + BATTLE_SHADOW_DEPTH_OFFSET;
        }

        const bool target_highlight = target_id && *target_id == sprite.unit_id && unit && unit->isAlive();
        if (auto* target_transform =
                battle_registry_.try_get<engine::component::TransformComponent>(sprite.target_shadow_entity);
            target_transform) {
            target_transform->position_ = world_position;
            target_transform->previous_position_ = world_position;
            target_transform->scale_ = glm::vec2{sprite.shadow_size.x * 1.16F, sprite.shadow_size.y * 1.2F};
            target_transform->rotation_ = 0.0F;
        }
        if (auto* target_render = battle_registry_.try_get<engine::component::RenderComponent>(sprite.target_shadow_entity);
            target_render) {
            target_render->color_ = target_highlight
                ? engine::utils::FColor{1.0F, 0.68F, 0.30F, 0.64F}
                : engine::utils::FColor{0.0F, 0.0F, 0.0F, 0.0F};
            target_render->depth_ = sprite.depth + pose_depth_offset + BATTLE_TARGET_SHADOW_DEPTH_OFFSET;
        }
    }
}

void BattleScene::syncEnemyHpBarHighlight() {
    std::optional<game::battle::BattleUnitId> target_id{};
    if (menu_model_.state == MenuState::TargetSelect) {
        if (const auto* entry = findTargetEntry(menu_model_.target_entry_cursor);
            entry && entry->enabled && !entry->is_ally) {
            target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
        }
    }

    battle_enemy_hp_bar_controller_.setHighlightedTarget(target_id);
}

void BattleScene::renderEnemyHpBars() {
    const auto& bars = battle_enemy_hp_bar_controller_.activeBars();
    if (bars.empty()) {
        return;
    }

    auto& renderer = context_.getRenderer();
    const auto& camera = context_.getCamera();
    const auto& config = battle_enemy_hp_bar_controller_.config();
    auto view = battle_registry_.view<BattleSpriteComponent, engine::component::SpriteComponent>();
    for (auto entity : view) {
        const auto& battle_sprite = view.get<BattleSpriteComponent>(entity);
        if (battle_sprite.side != game::battle::BattleSide::Enemy) {
            continue;
        }

        const auto* bar = battle_enemy_hp_bar_controller_.findBar(battle_sprite.unit_id);
        if (!bar || !bar->visible || bar->alpha <= 0.0F || config.size.x <= 0.0F || config.size.y <= 0.0F) {
            continue;
        }

        const auto& visual = view.get<engine::component::SpriteComponent>(entity);
        const glm::vec2 top_left = enemyHpBarScreenTopLeft(battle_sprite, visual, config);
        const glm::vec2 size = config.size;
        const float border = std::clamp(config.border_thickness, 0.0F, std::min(size.x, size.y) * 0.5F);
        const float alpha = std::clamp(bar->alpha, 0.0F, 1.0F);

        engine::utils::ColorOptions border_color{};
        border_color.use_gradient = false;
        border_color.start_color = bar->highlighted
            ? engine::utils::FColor{1.0F, 0.92F, 0.48F, alpha}
            : engine::utils::FColor{0.02F, 0.03F, 0.04F, alpha * 0.85F};
        border_color.end_color = border_color.start_color;
        renderer.drawFilledRect(screenRectToWorldRect(camera, top_left, size), &border_color);

        const glm::vec2 inner_top_left = top_left + glm::vec2{border, border};
        const glm::vec2 inner_size = glm::max(size - glm::vec2{border * 2.0F, border * 2.0F}, glm::vec2{0.0F});
        if (inner_size.x <= 0.0F || inner_size.y <= 0.0F) {
            continue;
        }

        engine::utils::ColorOptions background_color{};
        background_color.use_gradient = false;
        background_color.start_color = engine::utils::FColor{0.02F, 0.025F, 0.03F, alpha * 0.62F};
        background_color.end_color = background_color.start_color;
        renderer.drawFilledRect(screenRectToWorldRect(camera, inner_top_left, inner_size), &background_color);

        const float fill_width = inner_size.x * std::clamp(bar->display_ratio, 0.0F, 1.0F);
        if (fill_width <= 0.0F) {
            continue;
        }

        engine::utils::ColorOptions fill_color{};
        fill_color.use_gradient = false;
        fill_color.start_color = enemyHpBarFillColor(bar->display_ratio, alpha);
        fill_color.end_color = fill_color.start_color;
        renderer.drawFilledRect(screenRectToWorldRect(camera, inner_top_left, glm::vec2{fill_width, inner_size.y}),
                                &fill_color);
    }
}

void BattleScene::renderDamagePopups() {
    const auto& popups = battle_damage_popup_controller_.activePopups();
    if (popups.empty()) {
        return;
    }

    auto& text_renderer = context_.getTextRenderer();
    const auto& camera = context_.getCamera();
    for (const auto& popup : popups) {
        if (!popup.visible || popup.alpha <= 0.0F || popup.text.empty()) {
            continue;
        }

        engine::utils::LayoutOptions layout{};
        layout.glyph_scale = glm::vec2{popup.scale, popup.scale};
        const glm::vec2 text_size = text_renderer.getTextSize(popup.text,
                                                              engine::resource::defaults::UI_DEFAULT_FONT_ID,
                                                              DAMAGE_POPUP_FONT_SIZE_PX,
                                                              &layout);
        glm::vec2 screen_position = popup.screenPosition();
        screen_position.x -= text_size.x * 0.5F;
        screen_position.y -= text_size.y * 0.5F;

        engine::utils::TextRenderOverrides overrides{};
        overrides.color = battleDamagePopupColor(popup.kind, popup.alpha);
        overrides.shadow_enabled = true;
        overrides.shadow_offset = glm::vec2{1.0F, 1.0F};
        overrides.shadow_color = engine::utils::FColor{0.02F, 0.02F, 0.03F, std::min(popup.alpha, 0.78F)};
        overrides.glyph_scale = layout.glyph_scale;

        text_renderer.drawText(popup.text,
                               engine::resource::defaults::UI_DEFAULT_FONT_ID,
                               DAMAGE_POPUP_FONT_SIZE_PX,
                               camera.screenToWorld(screen_position),
                               text_renderer.getDefaultWorldStyleId(),
                               &overrides);
    }
}

void BattleScene::renderBattlefieldBackground() {
    auto& renderer = context_.getRenderer();
    const auto& camera = context_.getCamera();
    const glm::vec2 logical_size = camera.getLogicalSize();
    renderer.setAmbient(glm::vec3{1.0F, 1.0F, 1.0F});

    engine::utils::ColorOptions color{};
    color.use_gradient = false;

    color.start_color = engine::utils::FColor{0.06F, 0.08F, 0.12F, 1.0F};
    color.end_color = color.start_color;
    renderer.drawFilledRect(screenRectToWorldRect(camera, glm::vec2{0.0F, 0.0F}, logical_size), &color);

    battle_background_.render(renderer, camera);
}

void BattleScene::requestBattleEnd() {
    if (end_requested_) {
        return;
    }

    end_requested_ = true;

    game::defs::BattleEndedEvent event{};
    event.outcome = session_.outcome();
    event.final_units = session_.units();
    event.remaining_item_stocks = session_.itemStocks();
    event.reward_summary = victory_reward_summary_;
    context_.getDispatcher().trigger(event);

    requestPopScene();
}

void BattleScene::syncUserSettingsState() {
    auto* settings = presentation_options_.user_settings_service;
    if (!settings) {
        return;
    }
    const auto& snapshot = settings->snapshot();
    battle_animation_speed_ = snapshot.battle_animation_speed;
    cursor_memory_enabled_ = snapshot.cursor_memory;
    battle_damage_popup_controller_.setEnabled(snapshot.show_damage_popup);
    battle_enemy_hp_bar_controller_.setEnabled(snapshot.show_enemy_hp_bar);
}

void BattleScene::connectUserSettingsListeners() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::BattleAnimationSpeedChangedEvent>()
        .connect<&BattleScene::onBattleAnimationSpeedChanged>(this);
    dispatcher.sink<game::defs::DamagePopupVisibilityChangedEvent>()
        .connect<&BattleScene::onDamagePopupVisibilityChanged>(this);
    dispatcher.sink<game::defs::EnemyHpBarVisibilityChangedEvent>()
        .connect<&BattleScene::onEnemyHpBarVisibilityChanged>(this);
    dispatcher.sink<game::defs::CursorMemoryChangedEvent>()
        .connect<&BattleScene::onCursorMemoryChanged>(this);
}

void BattleScene::disconnectUserSettingsListeners() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::BattleAnimationSpeedChangedEvent>()
        .disconnect<&BattleScene::onBattleAnimationSpeedChanged>(this);
    dispatcher.sink<game::defs::DamagePopupVisibilityChangedEvent>()
        .disconnect<&BattleScene::onDamagePopupVisibilityChanged>(this);
    dispatcher.sink<game::defs::EnemyHpBarVisibilityChangedEvent>()
        .disconnect<&BattleScene::onEnemyHpBarVisibilityChanged>(this);
    dispatcher.sink<game::defs::CursorMemoryChangedEvent>()
        .disconnect<&BattleScene::onCursorMemoryChanged>(this);
}

void BattleScene::onBattleAnimationSpeedChanged(const game::defs::BattleAnimationSpeedChangedEvent& evt) {
    battle_animation_speed_ = evt.new_speed;
}

void BattleScene::onDamagePopupVisibilityChanged(const game::defs::DamagePopupVisibilityChangedEvent& evt) {
    battle_damage_popup_controller_.setEnabled(evt.visible);
}

void BattleScene::onEnemyHpBarVisibilityChanged(const game::defs::EnemyHpBarVisibilityChangedEvent& evt) {
    battle_enemy_hp_bar_controller_.setEnabled(evt.visible);
}

void BattleScene::onCursorMemoryChanged(const game::defs::CursorMemoryChangedEvent& evt) {
    cursor_memory_enabled_ = evt.enabled;
    if (!cursor_memory_enabled_) {
        last_actor_command_index_per_actor_.clear();
        last_skill_id_per_actor_.clear();
        last_item_id_per_actor_.clear();
        last_target_unit_id_per_actor_.clear();
    }
}

} // namespace game::scene
