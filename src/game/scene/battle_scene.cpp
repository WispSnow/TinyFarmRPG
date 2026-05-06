#include "battle_scene.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "game/battle/battle_ai_planner.h"
#include "game/component/appearance_component.h"
#include "game/defs/events.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"
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
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr float RESULT_HOLD_SECONDS = 0.20f;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/battle.rml";
constexpr std::string_view MODEL_NAME = "battle_scene";
constexpr int MAIN_ACTION_COLUMNS = 2;
constexpr float BATTLEFIELD_HEIGHT = 256.0f;
constexpr float BATTLE_SPRITE_SCALE_MULTIPLIER = 0.70f;
constexpr int BATTLE_RENDER_LAYER = 40;

enum class MainActionId : int {
    Attack = 1,
    Skill = 2,
    Item = 3,
    Guard = 4,
    Escape = 5,
    EndTurn = 6
};

struct BattleSpriteComponent {
    game::battle::BattleUnitId unit_id{0};
    game::battle::BattleSide side{game::battle::BattleSide::Player};
    glm::vec2 screen_position{0.0f};
    float scale{1.0f};
    float depth{0.0f};
    glm::vec2 shadow_size{56.0f, 4.0f};

    BattleSpriteComponent() = default;
    BattleSpriteComponent(game::battle::BattleUnitId unit_id,
                          game::battle::BattleSide side,
                          glm::vec2 screen_position,
                          float scale,
                          float depth,
                          glm::vec2 shadow_size)
        : unit_id(unit_id),
          side(side),
          screen_position(screen_position),
          scale(scale),
          depth(depth),
          shadow_size(shadow_size) {}
};

struct BattleFormationSlot {
    glm::vec2 screen_position{0.0F};
    float scale{1.0F};
    float depth{0.0F};
    glm::vec2 shadow_size{56.0F, 4.0F};
};

[[nodiscard]] std::string formatRecoveryText(const game::battle::BattleActionResult& result) {
    std::string text;
    if (result.hp_recovered > 0) {
        text = "recovered " + std::to_string(result.hp_recovered) + " HP";
    }
    if (result.mp_recovered > 0) {
        if (!text.empty()) {
            text += ", ";
            text += std::to_string(result.mp_recovered) + " MP";
        } else {
            text = "recovered " + std::to_string(result.mp_recovered) + " MP";
        }
    }
    return text;
}

[[nodiscard]] std::string formatActionResultText(const game::battle::BattleActionResult& result) {
    if (result.status == game::battle::BattleActionStatus::Rejected) {
        return result.failure_reason.empty() ? "Result: Action rejected" : "Result: " + result.failure_reason;
    }

    const std::string recovery_text = formatRecoveryText(result);
    switch (result.action_type) {
        case game::battle::BattleActionType::Attack: {
            std::string result_text = "Result: Attack dealt " + std::to_string(result.damage) + " dmg";
            if (result.target_defeated) {
                result_text += " (KO)";
            }
            return result_text;
        }
        case game::battle::BattleActionType::Skill: {
            std::string result_text = "Result: Skill";
            if (result.missed) {
                result_text += " missed";
                return result_text;
            }

            bool has_effect = false;
            if (result.damage > 0) {
                result_text += " dealt " + std::to_string(result.damage) + " dmg";
                has_effect = true;
            }
            if (!recovery_text.empty()) {
                result_text += has_effect ? ", " : " ";
                result_text += recovery_text;
                has_effect = true;
            }
            if (!result.states_added.empty()) {
                result_text += has_effect ? " " : " applied ";
                result_text += "+" + result.states_added.front();
                has_effect = true;
            }
            if (!has_effect) {
                result_text += " applied";
            }
            return result_text;
        }
        case game::battle::BattleActionType::Item:
            return recovery_text.empty() ? "Result: Item used" : "Result: Item " + recovery_text;
        case game::battle::BattleActionType::Guard:
            return "Result: Guarding";
        case game::battle::BattleActionType::Escape:
            return result.escape_succeeded ? "Result: Escaped" : "Result: Escape failed";
        case game::battle::BattleActionType::EndTurn:
            return "Result: Turn ended";
    }

    return "Result: Action applied";
}

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

[[nodiscard]] Rml::String ratioPercentString(int value, int max_value) {
    const float ratio = max_value > 0
        ? std::clamp(static_cast<float>(value) / static_cast<float>(max_value), 0.0f, 1.0f)
        : 0.0f;
    return std::to_string(static_cast<int>(std::round(ratio * 100.0f))) + "%";
}

[[nodiscard]] BattleFormationSlot battleFormationSlot(game::battle::BattleSide side,
                                                      std::size_t side_index,
                                                      std::size_t side_count,
                                                      float visual_scale) {
    const float centered = static_cast<float>(side_index) - (static_cast<float>(side_count) - 1.0F) * 0.5F;
    const bool is_player = side == game::battle::BattleSide::Player;
    const glm::vec2 base = is_player ? glm::vec2{480.0F, 140.0F} : glm::vec2{160.0F, 140.0F};
    const glm::vec2 step = is_player ? glm::vec2{18.0F, 28.0F} : glm::vec2{-18.0F, 30.0F};
    glm::vec2 position = base + centered * step;
    position.y = std::clamp(position.y, 58.0F, BATTLEFIELD_HEIGHT - 38.0F);

    const float shadow_width = std::clamp(30.0F * visual_scale, 34.0F, 58.0F);
    return BattleFormationSlot{
        .screen_position = position,
        .scale = visual_scale,
        .depth = position.y,
        .shadow_size = glm::vec2{shadow_width, 4.0F}
    };
}

[[nodiscard]] Rml::String portraitDecoratorForUnit(const game::battle::BattleUnit& unit) {
    if (unit.source_actor_id) {
        if (*unit.source_actor_id == "actor.player") {
            return "image(portrait-player)";
        }
        if (*unit.source_actor_id == "actor.lyria") {
            return "image(portrait-lyria)";
        }
        if (*unit.source_actor_id == "actor.tori") {
            return "image(portrait-tori)";
        }
    }

    if (unit.portrait.valid()) {
        if (unit.portrait.path.ends_with("/1.png")) {
            return "image(portrait-player)";
        }
        if (unit.portrait.path.ends_with("/9.png")) {
            return "image(portrait-lyria)";
        }
        if (unit.portrait.path.ends_with("/2.png")) {
            return "image(portrait-tori)";
        }
    }

    return "none";
}

[[nodiscard]] engine::utils::Rect screenRectToWorldRect(const engine::render::Camera& camera,
                                                        const glm::vec2& position,
                                                        const glm::vec2& size) {
    const glm::vec2 top_left = camera.screenToWorld(position);
    const glm::vec2 bottom_right = camera.screenToWorld(position + size);
    return engine::utils::Rect{top_left, bottom_right - top_left};
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
      session_(std::move(units), std::move(session_options)),
      presentation_options_(std::move(presentation_options)) {
}

BattleScene::~BattleScene() {
    disconnectInputListeners();
    shutdownUI();
}

bool BattleScene::init() {
    context_.getInputManager().pushContext(engine::input::InputContextId::Battle);
    context_pushed_ = true;

    if (!initUI()) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
        return false;
    }

    if (!initPresentation()) {
        shutdownUI();
        context_.getInputManager().popContext();
        context_pushed_ = false;
        return false;
    }

    if (!Scene::init()) {
        shutdownUI();
        context_.getInputManager().popContext();
        context_pushed_ = false;
        return false;
    }

    connectInputListeners();

    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        state_ = FlowState::BattleEnd;
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
    runStateMachine(delta_time);
    refreshView();
}

void BattleScene::render(float interpolation_alpha) {
    Scene::render(interpolation_alpha);
    context_.getRenderer().beginFrame(context_.getCamera());
    renderBattlefieldBackground();
    refreshPresentation();
    syncPresentationTransforms();
    battle_render_system_.renderPrepared(battle_registry_, context_.getRenderer(), interpolation_alpha);
}

void BattleScene::prepareUi(float interpolation_alpha) {
    Scene::prepareUi(interpolation_alpha);
    syncMenuFocus();
}

void BattleScene::clean() {
    disconnectInputListeners();
    shutdownUI();
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
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

    populateMainActions();

    if (!constructor.Bind("turn_text", &turn_text_) ||
        !constructor.Bind("result_text", &result_text_) ||
        !constructor.Bind("actions_enabled", &actions_enabled_) ||
        !constructor.Bind("back_hint", &back_hint_) ||
        !constructor.Bind("list_empty_text", &list_empty_text_) ||
        !constructor.Bind("target_empty_text", &target_empty_text_) ||
        !constructor.Bind("main_menu_visible", &main_menu_visible_) ||
        !constructor.Bind("list_menu_visible", &list_menu_visible_) ||
        !constructor.Bind("target_menu_visible", &target_menu_visible_) ||
        !constructor.Bind("list_empty", &list_empty_) ||
        !constructor.Bind("target_empty", &target_empty_) ||
        !constructor.Bind("party_status", &party_status_) ||
        !constructor.Bind("main_actions", &main_actions_) ||
        !constructor.Bind("list_entries", &list_entries_) ||
        !constructor.Bind("target_entries", &target_entries_)) {
        spdlog::error("BattleScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(
            constructor,
            "main_action_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                handleMainAction(getSingleIntArgument(arguments));
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
    menu_focus_dirty_ = true;
    return true;
}

void BattleScene::shutdownUI() {
    document_controller_.unload();
}

bool BattleScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto action_handle = constructor.RegisterStruct<MainActionViewModel>()) {
        action_handle.RegisterMember("action_id", &MainActionViewModel::action_id);
        action_handle.RegisterMember("entry_index", &MainActionViewModel::entry_index);
        action_handle.RegisterMember("label", &MainActionViewModel::label);
        action_handle.RegisterMember("enabled", &MainActionViewModel::enabled);
    } else {
        return false;
    }

    if (auto entry_handle = constructor.RegisterStruct<ListEntryViewModel>()) {
        entry_handle.RegisterMember("entry_index", &ListEntryViewModel::entry_index);
        entry_handle.RegisterMember("entry_id", &ListEntryViewModel::entry_id);
        entry_handle.RegisterMember("label", &ListEntryViewModel::label);
        entry_handle.RegisterMember("sublabel", &ListEntryViewModel::sublabel);
        entry_handle.RegisterMember("enabled", &ListEntryViewModel::enabled);
    } else {
        return false;
    }

    if (auto target_handle = constructor.RegisterStruct<TargetEntryViewModel>()) {
        target_handle.RegisterMember("entry_index", &TargetEntryViewModel::entry_index);
        target_handle.RegisterMember("unit_id", &TargetEntryViewModel::unit_id);
        target_handle.RegisterMember("label", &TargetEntryViewModel::label);
        target_handle.RegisterMember("enabled", &TargetEntryViewModel::enabled);
        target_handle.RegisterMember("is_ally", &TargetEntryViewModel::is_ally);
        target_handle.RegisterMember("is_dead", &TargetEntryViewModel::is_dead);
    } else {
        return false;
    }

    if (auto party_handle = constructor.RegisterStruct<PartyStatusViewModel>()) {
        party_handle.RegisterMember("unit_id", &PartyStatusViewModel::unit_id);
        party_handle.RegisterMember("name", &PartyStatusViewModel::name);
        party_handle.RegisterMember("hp_text", &PartyStatusViewModel::hp_text);
        party_handle.RegisterMember("mp_text", &PartyStatusViewModel::mp_text);
        party_handle.RegisterMember("hp_ratio_percent", &PartyStatusViewModel::hp_ratio_percent);
        party_handle.RegisterMember("mp_ratio_percent", &PartyStatusViewModel::mp_ratio_percent);
        party_handle.RegisterMember("portrait_decorator", &PartyStatusViewModel::portrait_decorator);
        party_handle.RegisterMember("active", &PartyStatusViewModel::active);
        party_handle.RegisterMember("ko", &PartyStatusViewModel::ko);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(main_actions_)>() ||
        !constructor.RegisterArray<decltype(list_entries_)>() ||
        !constructor.RegisterArray<decltype(target_entries_)>() ||
        !constructor.RegisterArray<decltype(party_status_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void BattleScene::connectInputListeners() {
    if (input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).connect<&BattleScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).connect<&BattleScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).connect<&BattleScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).connect<&BattleScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).connect<&BattleScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).connect<&BattleScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = true;
}

void BattleScene::disconnectInputListeners() {
    if (!input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).disconnect<&BattleScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).disconnect<&BattleScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).disconnect<&BattleScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).disconnect<&BattleScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).disconnect<&BattleScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).disconnect<&BattleScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = false;
}

void BattleScene::runStateMachine(float delta_time) {
    bool keep_running = true;
    while (keep_running) {
        keep_running = false;

        switch (state_) {
            case FlowState::WaitingForInput:
                return;
            case FlowState::ExecutingAction: {
                if (!pending_action_) {
                    beginCurrentTurnFlow();
                    keep_running = true;
                    break;
                }

                last_action_result_ = session_.submitAction(*pending_action_);
                pending_action_.reset();
                animation_timer_ = RESULT_HOLD_SECONDS;
                leaveInputMenu();
                state_ = FlowState::AnimatingResult;
                keep_running = true;
                break;
            }
            case FlowState::AnimatingResult: {
                animation_timer_ -= delta_time;
                if (animation_timer_ <= 0.0f) {
                    state_ = FlowState::CheckVictory;
                    keep_running = true;
                }
                break;
            }
            case FlowState::CheckVictory:
                state_ = (session_.outcome() == game::battle::BattleOutcome::Ongoing)
                    ? FlowState::NextTurn
                    : FlowState::BattleEnd;
                keep_running = true;
                break;
            case FlowState::NextTurn:
                beginCurrentTurnFlow();
                keep_running = true;
                break;
            case FlowState::BattleEnd:
                leaveInputMenu();
                requestBattleEnd();
                return;
        }
    }
}

void BattleScene::beginCurrentTurnFlow() {
    pending_action_.reset();

    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        state_ = FlowState::BattleEnd;
        leaveInputMenu();
        return;
    }

    const auto* actor = currentActor();
    if (!actor) {
        state_ = FlowState::BattleEnd;
        leaveInputMenu();
        return;
    }

    if (actor->side == game::battle::BattleSide::Enemy) {
        submitAction(buildEnemyAction(*actor));
        return;
    }

    state_ = FlowState::WaitingForInput;
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

void BattleScene::refreshView() {
    const auto current_actor_id = session_.currentActorId();

    std::string turn_text = "Turn: -";
    if (current_actor_id) {
        if (const auto* actor = session_.findUnit(*current_actor_id)) {
            turn_text = "Turn: " + actor->name + " (" + std::string(game::battle::toString(actor->side)) + ")";
        }
    }
    if (updateBoundString(turn_text_, turn_text)) {
        document_controller_.markDirty("turn_text");
    }

    rebuildPartyStatusView();

    std::string result_text = "Result: " + menu_status_text_;
    if (session_.outcome() != game::battle::BattleOutcome::Ongoing) {
        result_text = "Result: " + std::string(game::battle::toString(session_.outcome()));
    } else if (last_action_result_) {
        result_text = formatActionResultText(*last_action_result_);
    }
    if (updateBoundString(result_text_, result_text)) {
        document_controller_.markDirty("result_text");
    }

    const bool can_submit_action =
        !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        current_actor_id.has_value();

    if (updateBoundBool(actions_enabled_, can_submit_action)) {
        document_controller_.markDirty("actions_enabled");
    }

    refreshMenuEnabledState(can_submit_action);
    if (!can_submit_action && menu_state_ != MenuState::None) {
        leaveInputMenu();
    } else if (can_submit_action && menu_state_ == MenuState::None) {
        enterInputMenu();
    }
}

void BattleScene::rebuildPartyStatusView() {
    const auto current_actor_id = session_.currentActorId();
    std::vector<PartyStatusViewModel> next_party_status;

    for (const auto& unit : session_.units()) {
        if (unit.side != game::battle::BattleSide::Player) {
            continue;
        }

        next_party_status.push_back(PartyStatusViewModel{
            .unit_id = static_cast<int>(unit.id),
            .name = makeRmlString(unit.name),
            .hp_text = makeRmlString(std::to_string(std::max(0, unit.hp)) + "/" + std::to_string(std::max(0, unit.max_hp))),
            .mp_text = makeRmlString(std::to_string(std::max(0, unit.mp)) + "/" + std::to_string(std::max(0, unit.max_mp))),
            .hp_ratio_percent = ratioPercentString(unit.hp, unit.max_hp),
            .mp_ratio_percent = ratioPercentString(unit.mp, unit.max_mp),
            .portrait_decorator = portraitDecoratorForUnit(unit),
            .active = current_actor_id.has_value() && *current_actor_id == unit.id,
            .ko = !unit.isAlive()
        });
    }

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
        party_status_ = std::move(next_party_status);
        document_controller_.markDirty("party_status");
    }
}

void BattleScene::refreshMenuEnabledState(bool enabled) {
    bool changed = false;
    for (auto& action : main_actions_) {
        if (action.enabled == enabled) {
            continue;
        }

        action.enabled = enabled;
        changed = true;
    }

    if (changed) {
        document_controller_.markDirty("main_actions");
        menu_focus_dirty_ = true;
    }
}

void BattleScene::markMenuDirty() {
    document_controller_.markDirty("result_text");
    document_controller_.markDirty("back_hint");
    document_controller_.markDirty("list_empty_text");
    document_controller_.markDirty("target_empty_text");
    document_controller_.markDirty("main_menu_visible");
    document_controller_.markDirty("list_menu_visible");
    document_controller_.markDirty("target_menu_visible");
    document_controller_.markDirty("list_empty");
    document_controller_.markDirty("target_empty");
    document_controller_.markDirty("main_actions");
    document_controller_.markDirty("list_entries");
    document_controller_.markDirty("target_entries");
}

void BattleScene::enterInputMenu() {
    action_draft_ = {};
    setMenuState(MenuState::MainMenu);
}

void BattleScene::leaveInputMenu() {
    action_draft_ = {};
    setMenuState(MenuState::None);
}

void BattleScene::setMenuState(MenuState next_state) {
    menu_state_ = next_state;
    main_menu_visible_ = next_state == MenuState::MainMenu;
    list_menu_visible_ = next_state == MenuState::SkillList || next_state == MenuState::ItemList;
    target_menu_visible_ = next_state == MenuState::TargetSelect;
    list_empty_ = list_entries_.empty();
    target_empty_ = target_entries_.empty();

    switch (next_state) {
        case MenuState::None:
            menu_status_text_ = "Choose action";
            back_hint_ = "";
            break;
        case MenuState::MainMenu:
            menu_status_text_ = "Choose action";
            back_hint_ = "";
            main_action_cursor_ = main_actions_.empty()
                ? -1
                : std::clamp(main_action_cursor_, 0, static_cast<int>(main_actions_.size()) - 1);
            break;
        case MenuState::SkillList:
            menu_status_text_ = "Choose a skill";
            back_hint_ = "Cancel: Back";
            list_empty_text_ = "No skills available";
            list_entry_cursor_ = list_entries_.empty() ? -1 : std::clamp(list_entry_cursor_, 0, static_cast<int>(list_entries_.size()) - 1);
            break;
        case MenuState::ItemList:
            menu_status_text_ = "Choose an item";
            back_hint_ = "Cancel: Back";
            list_empty_text_ = "No battle items available";
            list_entry_cursor_ = list_entries_.empty() ? -1 : std::clamp(list_entry_cursor_, 0, static_cast<int>(list_entries_.size()) - 1);
            break;
        case MenuState::TargetSelect:
            menu_status_text_ = "Choose a target";
            back_hint_ = "Cancel: Back";
            target_empty_text_ = "No targets available";
            target_entry_cursor_ = target_entries_.empty() ? -1 : std::clamp(target_entry_cursor_, 0, static_cast<int>(target_entries_.size()) - 1);
            break;
    }

    markMenuDirty();
    menu_focus_dirty_ = true;
}

void BattleScene::syncMenuFocus() {
    if (!menu_focus_dirty_) {
        return;
    }

    int cursor = -1;
    std::string_view prefix;

    switch (menu_state_) {
        case MenuState::None:
            menu_focus_dirty_ = false;
            return;
        case MenuState::MainMenu:
            cursor = main_action_cursor_;
            prefix = "battle-main-action-";
            break;
        case MenuState::SkillList:
        case MenuState::ItemList:
            cursor = list_entry_cursor_;
            prefix = "battle-list-entry-";
            break;
        case MenuState::TargetSelect:
            cursor = target_entry_cursor_;
            prefix = "battle-target-entry-";
            break;
    }

    // cursor < 0: 无可聚焦条目，直接清除脏标记。
    // cursor >= 0 且 focus 成功: 清除。
    // cursor >= 0 但元素尚未生成（data-if 子树未展开）: 保持脏标记，下帧重试。
    if (cursor < 0 || focusElementById(makeElementId(prefix, cursor))) {
        menu_focus_dirty_ = false;
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
    return true;
}

void BattleScene::populateMainActions() {
    const bool enabled = actions_enabled_;
    main_actions_ = {
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Attack), .entry_index = 0, .label = "Attack", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Skill), .entry_index = 1, .label = "Skill", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Item), .entry_index = 2, .label = "Item", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Guard), .entry_index = 3, .label = "Guard", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::Escape), .entry_index = 4, .label = "Escape", .enabled = enabled},
        MainActionViewModel{.action_id = static_cast<int>(MainActionId::EndTurn), .entry_index = 5, .label = "End Turn", .enabled = enabled},
    };
}

void BattleScene::populateSkillEntries(const game::battle::BattleUnit& actor) {
    list_entries_.clear();
    list_entry_cursor_ = -1;
    list_empty_text_ = "No skills available";

    if (!rpg_catalog_) {
        spdlog::warn("BattleScene: RPG catalog 不可用，无法生成技能列表。");
        return;
    }

    int entry_index = 0;
    for (const auto& skill_id : actor.skill_ids) {
        const auto* skill = rpg_catalog_->findSkill(skill_id);
        if (!skill) {
            spdlog::warn("BattleScene: skill '{}' 不存在于 RPG catalog，已跳过。", skill_id);
            continue;
        }

        const std::string_view label = skill->display_name_.empty()
            ? std::string_view{skill->id_}
            : std::string_view{skill->display_name_};
        list_entries_.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = skill->id_,
            .label = makeRmlString(label),
            .sublabel = skillSubtitle(actor, *skill),
            .enabled = isSkillEntryEnabled(actor, *skill)
        });
    }

    list_entry_cursor_ = firstEnabledListEntryIndex();
}

void BattleScene::populateItemEntries() {
    list_entries_.clear();
    list_entry_cursor_ = -1;
    list_empty_text_ = "No battle items available";

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
        list_entries_.push_back(ListEntryViewModel{
            .entry_index = entry_index++,
            .entry_id = item->id_str_,
            .label = makeRmlString(label),
            .sublabel = itemSubtitle(stock_it->second, *item->battle_use_),
            .enabled = isItemEntryEnabled(stock_it->second, *item->battle_use_)
        });
    }

    list_entry_cursor_ = firstEnabledListEntryIndex();
}

const BattleScene::ListEntryViewModel* BattleScene::findListEntry(int entry_index) const {
    const auto it = std::find_if(
        list_entries_.begin(),
        list_entries_.end(),
        [entry_index](const ListEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == list_entries_.end() ? nullptr : &*it;
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
    for (const auto& entry : list_entries_) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return list_entries_.empty() ? -1 : list_entries_.front().entry_index;
}

void BattleScene::populateTargetEntries(game::data::Scope scope, const game::battle::BattleUnit& actor) {
    target_entries_.clear();
    target_entry_cursor_ = -1;
    target_empty_text_ = "No valid targets";

    int entry_index = 0;
    for (const auto& unit : session_.units()) {
        const bool matches_scope = (scope == game::data::Scope::OneEnemy && unit.side != actor.side) ||
            (scope == game::data::Scope::OneAlly && unit.side == actor.side);
        if (!matches_scope) {
            continue;
        }

        target_entries_.push_back(TargetEntryViewModel{
            .entry_index = entry_index++,
            .unit_id = static_cast<int>(unit.id),
            .label = targetLabel(unit),
            .enabled = unit.isAlive(),
            .is_ally = unit.side == actor.side,
            .is_dead = !unit.isAlive()
        });
    }

    target_entry_cursor_ = firstEnabledTargetEntryIndex();
}

const BattleScene::TargetEntryViewModel* BattleScene::findTargetEntry(int entry_index) const {
    const auto it = std::find_if(
        target_entries_.begin(),
        target_entries_.end(),
        [entry_index](const TargetEntryViewModel& entry) {
            return entry.entry_index == entry_index;
        });
    return it == target_entries_.end() ? nullptr : &*it;
}

int BattleScene::firstEnabledTargetEntryIndex() const {
    for (const auto& entry : target_entries_) {
        if (entry.enabled) {
            return entry.entry_index;
        }
    }
    return target_entries_.empty() ? -1 : target_entries_.front().entry_index;
}

Rml::String BattleScene::targetLabel(const game::battle::BattleUnit& unit) const {
    std::string label = unit.name + " HP " + std::to_string(unit.hp) + "/" + std::to_string(unit.max_hp);
    if (!unit.isAlive()) {
        label += " (KO)";
    }
    return label;
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
            return MenuState::MainMenu;
    }
    return MenuState::MainMenu;
}

void BattleScene::setMenuHint(std::string_view text) {
    menu_status_text_ = std::string{text};
    document_controller_.markDirty("result_text");
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

void BattleScene::handleMainAction(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(main_actions_.size())) {
        return;
    }

    main_action_cursor_ = entry_index;
    menu_focus_dirty_ = true;
    const auto& action = main_actions_[entry_index];
    if (!action.enabled) {
        return;
    }

    switch (static_cast<MainActionId>(action.action_id)) {
        case MainActionId::Attack:
            queueAttackAction();
            break;
        case MainActionId::Skill:
            queueSkillAction();
            break;
        case MainActionId::Item:
            queueItemAction();
            break;
        case MainActionId::Guard:
            queueGuardAction();
            break;
        case MainActionId::Escape:
            queueEscapeAction();
            break;
        case MainActionId::EndTurn:
            queueEndTurnAction();
            break;
    }
}

void BattleScene::handleListEntry(int entry_index) {
    if (!isWaitingForActionInput() || entry_index < 0 || entry_index >= static_cast<int>(list_entries_.size())) {
        return;
    }

    const auto* entry = findListEntry(entry_index);
    if (!entry) {
        return;
    }

    list_entry_cursor_ = entry->entry_index;
    menu_focus_dirty_ = true;
    if (!entry->enabled) {
        return;
    }

    if (menu_state_ == MenuState::SkillList) {
        handleSkillEntry(*entry);
    } else if (menu_state_ == MenuState::ItemList) {
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

    target_entry_cursor_ = entry->entry_index;
    menu_focus_dirty_ = true;
    if (!entry->enabled) {
        return;
    }

    action_draft_.selected_target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
    (void)submitDraftAction();
}

bool BattleScene::submitDraftAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        setMenuHint("Action is no longer available.");
        return false;
    }

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
    state_ = FlowState::ExecutingAction;
}

bool BattleScene::isWaitingForActionInput() const {
    return !end_requested_ &&
        state_ == FlowState::WaitingForInput &&
        session_.outcome() == game::battle::BattleOutcome::Ongoing &&
        session_.currentActorId().has_value();
}

bool BattleScene::moveMenuCursor(int delta) {
    if (!isWaitingForActionInput() || delta == 0) {
        return false;
    }

    switch (menu_state_) {
        case MenuState::MainMenu: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(main_actions_.size());
            for (const auto& action : main_actions_) {
                enabled_entries.push_back(action.enabled);
            }

            if (!moveCursorInEntries(main_action_cursor_, static_cast<int>(main_actions_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::SkillList:
        case MenuState::ItemList: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(list_entries_.size());
            for (const auto& entry : list_entries_) {
                enabled_entries.push_back(entry.enabled);
            }

            if (!moveCursorInEntries(list_entry_cursor_, static_cast<int>(list_entries_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
            syncMenuFocus();
            return true;
        }
        case MenuState::TargetSelect: {
            std::vector<bool> enabled_entries;
            enabled_entries.reserve(target_entries_.size());
            for (const auto& target : target_entries_) {
                enabled_entries.push_back(target.enabled);
            }

            if (!moveCursorInEntries(target_entry_cursor_, static_cast<int>(target_entries_.size()), delta, enabled_entries)) {
                return false;
            }
            menu_focus_dirty_ = true;
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

bool BattleScene::onMenuUpPressed() {
    const bool moved = moveMenuCursor(menu_state_ == MenuState::MainMenu ? -MAIN_ACTION_COLUMNS : -1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuDownPressed() {
    const bool moved = moveMenuCursor(menu_state_ == MenuState::MainMenu ? MAIN_ACTION_COLUMNS : 1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuLeftPressed() {
    const bool moved = moveMenuCursor(-1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuRightPressed() {
    const bool moved = moveMenuCursor(1);
    return moved || menu_state_ != MenuState::None;
}

bool BattleScene::onMenuConfirmPressed() {
    if (!isWaitingForActionInput()) {
        return menu_state_ != MenuState::None;
    }

    switch (menu_state_) {
        case MenuState::MainMenu:
            handleMainAction(main_action_cursor_);
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            handleListEntry(list_entry_cursor_);
            return true;
        case MenuState::TargetSelect:
            handleTargetEntry(target_entry_cursor_);
            return true;
        case MenuState::None:
            return false;
    }
}

bool BattleScene::onMenuCancelPressed() {
    if (!isWaitingForActionInput()) {
        return menu_state_ != MenuState::None;
    }

    switch (menu_state_) {
        case MenuState::TargetSelect:
            action_draft_.selected_target_id.reset();
            setMenuState(menuStateForActionDraftSource());
            return true;
        case MenuState::SkillList:
        case MenuState::ItemList:
            action_draft_ = {};
            setMenuState(MenuState::MainMenu);
            return true;
        case MenuState::MainMenu:
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

void BattleScene::queueEndTurnAction() {
    game::battle::BattleUnitId actor_id = 0;
    if (!prepareActionActor(actor_id)) {
        return;
    }

    submitAction(game::battle::BattleAction{
        .type = game::battle::BattleActionType::EndTurn,
        .actor_id = actor_id,
        .target_id = std::nullopt
    });
}

const game::battle::BattleUnit* BattleScene::prepareActionActor(game::battle::BattleUnitId& out_actor_id) const {
    if (state_ != FlowState::WaitingForInput || session_.outcome() != game::battle::BattleOutcome::Ongoing) {
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

bool BattleScene::initPresentation() {
    auto& resource_manager = context_.getResourceManager();
    if (auto* resource_ptr = battle_registry_.ctx().find<engine::resource::ResourceManager*>()) {
        *resource_ptr = &resource_manager;
    } else {
        battle_registry_.ctx().emplace<engine::resource::ResourceManager*>(&resource_manager);
    }

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
        float scale = unit.side == game::battle::BattleSide::Player ? 2.0F : 1.8F;
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
                    blueprint_id = actor->map_actor_id_;
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
                    scale = enemy->battle_visual_.scale_;
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
        scale = std::clamp(scale * BATTLE_SPRITE_SCALE_MULTIPLIER, 0.95F, 1.45F);
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
        battle_registry_.emplace<engine::component::RenderComponent>(entity, BATTLE_RENDER_LAYER, formation_slot.depth);
        battle_registry_.emplace<BattleSpriteComponent>(
            entity,
            unit.id,
            unit.side,
            formation_slot.screen_position,
            formation_slot.scale,
            formation_slot.depth,
            formation_slot.shadow_size);

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
    refreshPresentation();
    return true;
}

void BattleScene::updatePresentation(float delta_time) {
    auto view = battle_registry_.view<engine::component::AnimationComponent, engine::component::SpriteComponent>();
    for (auto entity : view) {
        auto& animation = view.get<engine::component::AnimationComponent>(entity);
        auto& sprite = view.get<engine::component::SpriteComponent>(entity);
        advanceAnimation(animation, sprite, delta_time);
    }
}

void BattleScene::refreshPresentation() {
    std::optional<game::battle::BattleUnitId> target_id{};
    if (menu_state_ == MenuState::TargetSelect) {
        if (const auto* entry = findTargetEntry(target_entry_cursor_); entry && entry->enabled) {
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
        render.depth_ = sprite.depth;
    }
}

void BattleScene::syncPresentationTransforms() {
    const auto& camera = context_.getCamera();
    auto view = battle_registry_.view<BattleSpriteComponent, engine::component::TransformComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        const glm::vec2 position = camera.screenToWorld(sprite.screen_position);
        transform.position_ = position;
        transform.previous_position_ = position;
        transform.scale_ = glm::vec2{sprite.scale, sprite.scale};
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

    color.start_color = engine::utils::FColor{0.10F, 0.14F, 0.18F, 1.0F};
    color.end_color = color.start_color;
    renderer.drawFilledRect(screenRectToWorldRect(camera, glm::vec2{0.0F, BATTLEFIELD_HEIGHT - 4.0F}, glm::vec2{logical_size.x, 4.0F}), &color);

    const auto current_actor_id = session_.currentActorId();
    std::optional<game::battle::BattleUnitId> target_id{};
    if (menu_state_ == MenuState::TargetSelect) {
        if (const auto* entry = findTargetEntry(target_entry_cursor_); entry && entry->enabled) {
            target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id);
        }
    }

    auto view = battle_registry_.view<BattleSpriteComponent>();
    for (auto entity : view) {
        const auto& sprite = view.get<BattleSpriteComponent>(entity);
        const auto* unit = session_.findUnit(sprite.unit_id);
        if (unit && unit->isAlive()) {
            color.start_color = engine::utils::FColor{0.03F, 0.05F, 0.08F, 0.55F};
            color.end_color = color.start_color;
            renderer.drawFilledRect(
                screenRectToWorldRect(camera,
                                      sprite.screen_position + glm::vec2{-sprite.shadow_size.x * 0.5F, 13.0F},
                                      sprite.shadow_size),
                &color);
        }

        if ((!current_actor_id || *current_actor_id != sprite.unit_id) && (!target_id || *target_id != sprite.unit_id)) {
            continue;
        }

        color.start_color = target_id && *target_id == sprite.unit_id
            ? engine::utils::FColor{1.0F, 0.68F, 0.30F, 0.72F}
            : engine::utils::FColor{0.95F, 0.86F, 0.42F, 0.55F};
        color.end_color = color.start_color;
        renderer.drawFilledRect(
            screenRectToWorldRect(camera,
                                  sprite.screen_position + glm::vec2{-sprite.shadow_size.x * 0.5F, 13.0F},
                                  glm::vec2{sprite.shadow_size.x, 4.0F}),
            &color);
    }
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
    context_.getDispatcher().trigger(event);

    requestPopScene();
}

} // namespace game::scene
