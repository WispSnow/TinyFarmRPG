#include "engine/input/input_manager.h"
#include "engine/input/input_binding_config.h"
#include "engine/input/input_context_registry.h"
#include "engine/input/input_event_routing.h"
#include "engine/input/input_binding_tokens.h"
#include "engine/input/input_glyphs.h"

#include "engine/core/game_state.h"
#include "engine/utils/events.h"
#include "engine/utils/math.h"

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>
#ifdef TF_ENABLE_DEBUG_UI
#include <imgui.h>
#endif
#include <algorithm>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>

namespace engine::input {

namespace {

constexpr float AXIS_PRESS_THRESHOLD = 0.6f;
constexpr float AXIS_RELEASE_THRESHOLD = 0.4f;

template <typename KeyT>
void handleInputEdge(KeyT key,
                     std::unordered_map<KeyT, std::vector<entt::id_type>>& mapping,
                     std::unordered_map<KeyT, bool>& down_states,
                     std::unordered_map<entt::id_type, ActionEntry>& actions,
                     bool is_down,
                     const std::unordered_set<entt::id_type>* allowed_actions,
                     Uint64 timestamp_ms) {
    auto actions_it = mapping.find(key);
    if (actions_it == mapping.end()) {
        return;
    }

    auto& was_down = down_states[key];
    if (is_down == was_down) {
        return;
    }
    was_down = is_down;

    for (auto action_id : actions_it->second) {
        if (allowed_actions != nullptr && !allowed_actions->contains(action_id)) {
            continue;
        }

        auto& entry = actions[action_id];
        if (is_down) {
            ++entry.active_count;
            if (entry.active_count == 1) {
                entry.state = ActionState::PRESSED;
                entry.press_buffer.push(timestamp_ms);
            }
        } else if (entry.active_count > 0) {
            --entry.active_count;
            if (entry.active_count == 0) {
                entry.state = ActionState::RELEASED;
            }
        }
    }
}

[[nodiscard]] constexpr std::size_t gamepadButtonIndex(SDL_GamepadButton button) {
    return static_cast<std::size_t>(button);
}

[[nodiscard]] constexpr std::size_t gamepadAxisIndex(SDL_GamepadAxis axis) {
    return static_cast<std::size_t>(axis);
}

[[nodiscard]] constexpr std::size_t gamepadAxisDirectionIndex(GamepadAxisDirection direction) {
    return static_cast<std::size_t>(direction);
}

[[nodiscard]] std::vector<SDL_JoystickID> queryConnectedGamepadIds() {
    std::vector<SDL_JoystickID> result;
    if (SDL_WasInit(SDL_INIT_GAMEPAD) == 0) {
        return result;
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr || count <= 0) {
        return result;
    }

    result.assign(ids, ids + count);
    SDL_free(ids);
    return result;
}

#ifdef TF_ENABLE_DEBUG_UI
[[nodiscard]] bool isImGuiBlockingRmlUi(const SDL_Event& event) {
    if (!ImGui::GetCurrentContext()) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_TEXT_EDITING:
            return io.WantCaptureKeyboard;
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            return io.WantCaptureMouse;
        default:
            return false;
    }
}
#endif

} // namespace

std::unique_ptr<InputManager> InputManager::create(entt::dispatcher* dispatcher,
                                                   engine::core::GameState* game_state,
                                                   std::string_view config_path) {
    if (dispatcher == nullptr) {
        spdlog::error("创建 InputManager 失败：事件分发器为空指针。");
        return nullptr;
    }
    if (game_state == nullptr) {
        spdlog::error("创建 InputManager 失败：GameState 为空指针。");
        return nullptr;
    }
    return std::unique_ptr<InputManager>(new InputManager(dispatcher, game_state, config_path));
}

InputManager::InputManager(entt::dispatcher* dispatcher,
                           engine::core::GameState* game_state,
                           std::string_view config_path)
    : dispatcher_(dispatcher),
      game_state_(game_state),
      config_path_(config_path) {
    if (!loadConfig(config_path)) {
        initializeMappings(defaultInputMappings());
    }
    initializeConnectedGamepads();

    float x = 0.0f;
    float y = 0.0f;
    SDL_GetMouseState(&x, &y);
    mouse_position_ = {x, y};
    recalculateLogicalMousePosition();
    spdlog::trace("初始鼠标位置: ({}, {})", mouse_position_.x, mouse_position_.y);
}

InputManager::~InputManager() {
    closeActiveGamepad();
}

entt::sink<entt::sigh<bool()>> InputManager::onAction(entt::id_type action_name_id, ActionState action_state) {
    if (action_state == ActionState::INACTIVE) {
        spdlog::warn("InputManager::onAction: ActionState::INACTIVE 不能用于绑定回调，将回退到 PRESSED。");
        action_state = ActionState::PRESSED;
    }

    const auto index = static_cast<std::size_t>(action_state);
    if (index >= ActionEntry::CALLBACK_STATE_COUNT) {
        spdlog::warn("InputManager::onAction: 非法 ActionState={}，将回退到 PRESSED。", index);
        return actions_[action_name_id].signals[0];
    }

    return actions_[action_name_id].signals[index];
}

void InputManager::update() {
    consumeTick();
    sampleInputEvents();
    dispatchActionCallbacks();
}

void InputManager::sampleInputEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (rebind_capture_.active) {
            if (handleRebindCaptureEvent(event)) {
                continue;
            }

            if (isSystemEventDuringInputCapture(event)) {
                processEvent(event);
            }
            continue;
        }

        bool imgui_blocks_rmlui = false;
        // Observer 必须先运行。GLRenderer 会在这里把事件喂给 ImGui，
        // 这样后续 isImGuiBlockingRmlUi() 读取到的 WantCapture* 才对应当前事件后的状态。
        if (sdl_event_observer_) {
            sdl_event_observer_(event);
        }
#ifdef TF_ENABLE_DEBUG_UI
        imgui_blocks_rmlui = isImGuiBlockingRmlUi(event);
#endif

        bool should_propagate = true;
        if (!imgui_blocks_rmlui && rmlui_event_callback_ &&
            !shouldSuppressRmlUiKeyboardEvent(event, currentContext(), rmlui_suppressed_navigation_scancodes_)) {
            should_propagate = rmlui_event_callback_(event);
        }

        const bool always_propagate = shouldAlwaysPropagateAfterUi(event, currentContext());

        if (!should_propagate && !always_propagate) {
            continue;
        }

        processEvent(event);
    }
}

void InputManager::dispatchActionCallbacks() {
    // 根据当前上下文选择派发顺序：
    //   - 有激活上下文（Menu / Dialogue / Battle 等）时，只派发该上下文允许的动作子集，
    //     顺序由 InputContextDefinition::dispatch_actions 决定；
    //   - 无上下文时退回全局顺序 action_dispatch_order_（按 initializeMappings 中的注册顺序）。
    const auto* context_definition = currentContextDefinition();
    const auto& dispatch_order = context_definition != nullptr ? context_definition->dispatch_actions : action_dispatch_order_;

    for (auto action_id : dispatch_order) {
        auto action_it = actions_.find(action_id);
        if (action_it == actions_.end()) {
            continue;
        }

        auto& entry = action_it->second;
        if (entry.state != ActionState::INACTIVE) {
            entry.signals[static_cast<std::size_t>(entry.state)].collect([](bool result) {
                return result;
            });
        }
    }
}

void InputManager::consumeTick() {
    for (auto& [_, entry] : actions_) {
        if (entry.state == ActionState::PRESSED) {
            entry.state = ActionState::HELD;
        } else if (entry.state == ActionState::RELEASED) {
            entry.state = ActionState::INACTIVE;
        }
    }

    mouse_wheel_delta_ = {0.0f, 0.0f};
}

void InputManager::pushContext(InputContextId id) {
    clearAllInputState();
    context_stack_.push_back(id);
}

void InputManager::popContext() {
    if (context_stack_.empty()) {
        spdlog::warn("InputManager::popContext: 上下文栈为空，忽略 pop。");
        return;
    }

    clearAllInputState();
    context_stack_.pop_back();
}

std::optional<InputContextId> InputManager::currentContext() const {
    if (context_stack_.empty()) {
        return std::nullopt;
    }

    return context_stack_.back();
}

void InputManager::quit() {
    dispatcher_->trigger<engine::utils::QuitEvent>();
}

bool InputManager::isActionDown(entt::id_type action_name_id) const {
    if (auto it = actions_.find(action_name_id); it != actions_.end()) {
        return it->second.state == ActionState::PRESSED || it->second.state == ActionState::HELD;
    }
    return false;
}

bool InputManager::isActionPressed(entt::id_type action_name_id) const {
    if (auto it = actions_.find(action_name_id); it != actions_.end()) {
        return it->second.state == ActionState::PRESSED;
    }
    return false;
}

bool InputManager::isActionReleased(entt::id_type action_name_id) const {
    if (auto it = actions_.find(action_name_id); it != actions_.end()) {
        return it->second.state == ActionState::RELEASED;
    }
    return false;
}

glm::vec2 InputManager::getMousePosition() const {
    return mouse_position_;
}

glm::vec2 InputManager::getLogicalMousePosition() const {
    return logical_mouse_position_;
}

glm::vec2 InputManager::getMouseWheelDelta() const {
    return mouse_wheel_delta_;
}

InputDevice InputManager::getLastInputDevice() const {
    return last_input_device_;
}

GamepadDebugState InputManager::getGamepadDebugState() const {
    GamepadDebugState state;
    state.connected_gamepad_count = connected_gamepad_ids_.size();
    state.has_active_gamepad = active_gamepad_ != nullptr;
    state.active_gamepad_id = active_gamepad_id_;
    state.last_input_device = last_input_device_;
    state.button_states = gamepad_button_states_;
    state.axis_raw_values = gamepad_axis_raw_values_;
    state.axis_normalized_values = gamepad_axis_normalized_values_;
    if (active_gamepad_ != nullptr) {
        if (const char* name = SDL_GetGamepadName(active_gamepad_); name != nullptr) {
            state.active_gamepad_name = name;
        }
    }
    return state;
}

std::vector<BindingDefinition> InputManager::getActionBindings(entt::id_type action_name_id) const {
    if (const auto it = action_bindings_.find(action_name_id); it != action_bindings_.end()) {
        return it->second;
    }

    return {};
}

std::optional<ActionPrompt> InputManager::getActionPrompt(entt::id_type action_name_id) const {
    const auto bindings_it = action_bindings_.find(action_name_id);
    if (bindings_it == action_bindings_.end() || bindings_it->second.empty()) {
        return std::nullopt;
    }

    const auto& bindings = bindings_it->second;
    const auto exact_match = std::find_if(bindings.begin(), bindings.end(), [this](const BindingDefinition& binding) {
        return binding.device == last_input_device_;
    });
    if (exact_match != bindings.end()) {
        return makeActionPrompt(*exact_match);
    }

    if (last_input_device_ != InputDevice::Gamepad) {
        const auto non_gamepad_match = std::find_if(bindings.begin(), bindings.end(), [](const BindingDefinition& binding) {
            return binding.device != InputDevice::Gamepad;
        });
        if (non_gamepad_match != bindings.end()) {
            return makeActionPrompt(*non_gamepad_match);
        }
    }

    return makeActionPrompt(bindings.front());
}

bool InputManager::peekBufferedPress(entt::id_type action_name_id, Uint64 window_ms) const {
    if (const auto it = actions_.find(action_name_id); it != actions_.end()) {
        return it->second.press_buffer.peek(SDL_GetTicks(), window_ms);
    }

    return false;
}

bool InputManager::consumeBufferedPress(entt::id_type action_name_id, Uint64 window_ms) {
    if (const auto it = actions_.find(action_name_id); it != actions_.end()) {
        return it->second.press_buffer.consume(SDL_GetTicks(), window_ms);
    }

    return false;
}

InputDebugSnapshot InputManager::getDebugSnapshot(Uint64 now_ms) const {
    const Uint64 snapshot_time_ms = now_ms == 0 ? SDL_GetTicks() : now_ms;

    InputDebugSnapshot snapshot;
    snapshot.last_input_device = last_input_device_;
    snapshot.actions.reserve(actions_.size());

    std::unordered_set<entt::id_type> seen_actions;
    seen_actions.reserve(actions_.size());

    const auto append_action = [&](entt::id_type action_id, const ActionEntry& entry) {
        ActionDebugSnapshot action_snapshot;
        action_snapshot.id = action_id;
        action_snapshot.name = entry.name;
        action_snapshot.state = entry.state;
        action_snapshot.active_count = entry.active_count;
        action_snapshot.bindings = getActionBindings(action_id);
        action_snapshot.active_prompt = getActionPrompt(action_id);
        const auto buffered_entries = entry.press_buffer.snapshot(snapshot_time_ms);
        action_snapshot.buffered_presses.reserve(buffered_entries.size());
        for (const auto& buffered_entry : buffered_entries) {
            action_snapshot.buffered_presses.push_back(BufferedPressDebugEntry{
                .timestamp_ms = buffered_entry.timestamp_ms,
                .age_ms = buffered_entry.age_ms,
            });
        }
        snapshot.actions.push_back(std::move(action_snapshot));
        seen_actions.insert(action_id);
    };

    for (auto action_id : action_dispatch_order_) {
        if (const auto it = actions_.find(action_id); it != actions_.end()) {
            append_action(action_id, it->second);
        }
    }

    for (const auto& [action_id, entry] : actions_) {
        if (!seen_actions.contains(action_id)) {
            append_action(action_id, entry);
        }
    }

    snapshot.rumble = rumble_debug_state_;
    if (active_rumble_.has_value() && snapshot_time_ms < active_rumble_->ends_at_ms) {
        snapshot.rumble.active = true;
        snapshot.rumble.current_intensity = active_rumble_->intensity;
        snapshot.rumble.current_duration_ms = active_rumble_->duration_ms;
        snapshot.rumble.remaining_ms = active_rumble_->ends_at_ms - snapshot_time_ms;
    } else {
        snapshot.rumble.active = false;
        snapshot.rumble.current_intensity = 0.0f;
        snapshot.rumble.current_duration_ms = 0;
        snapshot.rumble.remaining_ms = 0;
    }

    snapshot.rebind.capture_active = rebind_capture_.active;
    snapshot.rebind.action_id = rebind_capture_.action_id;
    snapshot.rebind.binding_index = rebind_capture_.binding_index;
    if (const auto action_it = actions_.find(rebind_capture_.action_id); action_it != actions_.end()) {
        snapshot.rebind.action_name = action_it->second.name;
    }
    if (pending_rebind_conflict_.has_value()) {
        snapshot.rebind.pending_conflict = true;
        snapshot.rebind.pending_token = pending_rebind_conflict_->replacement.token;
        for (const auto& [conflict_action_id, _] : pending_rebind_conflict_->conflicts) {
            if (const auto action_it = actions_.find(conflict_action_id); action_it != actions_.end()) {
                snapshot.rebind.conflicting_action_names.push_back(action_it->second.name);
            }
        }
    }

    return snapshot;
}

bool InputManager::rumble(float intensity, Uint32 duration_ms) {
    rumble_debug_state_.has_last_request = true;
    rumble_debug_state_.last_intensity = std::clamp(intensity, 0.0f, 1.0f);
    rumble_debug_state_.last_duration_ms = duration_ms;
    rumble_debug_state_.last_request_succeeded = false;

    if (active_gamepad_ == nullptr || duration_ms == 0 || rumble_debug_state_.last_intensity <= 0.0f) {
        active_rumble_.reset();
        return false;
    }

    const auto amplitude = static_cast<Uint16>(std::clamp(rumble_debug_state_.last_intensity, 0.0f, 1.0f) * 65535.0f);
    const bool ok = SDL_RumbleGamepad(active_gamepad_, amplitude, amplitude, duration_ms);
    rumble_debug_state_.last_request_succeeded = ok;
    if (ok) {
        active_rumble_ = ActiveRumbleState{
            .intensity = rumble_debug_state_.last_intensity,
            .duration_ms = duration_ms,
            .ends_at_ms = SDL_GetTicks() + duration_ms,
        };
    } else {
        active_rumble_.reset();
    }

    return ok;
}

bool InputManager::beginRebindCapture(entt::id_type action_name_id, std::size_t binding_index) {
    const auto bindings_it = action_bindings_.find(action_name_id);
    if (bindings_it == action_bindings_.end() || binding_index > bindings_it->second.size()) {
        return false;
    }

    pending_rebind_conflict_.reset();
    clearAllInputState();
    rebind_capture_ = RebindCaptureState{
        .action_id = action_name_id,
        .binding_index = binding_index,
        .active = true,
    };
    return true;
}

void InputManager::cancelRebindCapture() {
    rebind_capture_ = RebindCaptureState{};
}

bool InputManager::confirmPendingRebindConflict() {
    if (!pending_rebind_conflict_.has_value()) {
        return false;
    }

    const auto pending = *pending_rebind_conflict_;
    pending_rebind_conflict_.reset();
    return applyBindingReplacement(pending.action_id, pending.binding_index, pending.replacement, pending.conflicts);
}

void InputManager::discardPendingRebindConflict() {
    pending_rebind_conflict_.reset();
}

void InputManager::setSdlEventObserver(std::function<void(const SDL_Event&)> callback) {
    sdl_event_observer_ = std::move(callback);
}

void InputManager::setRmlUiEventForwarder(std::function<bool(SDL_Event&)> callback) {
    rmlui_event_callback_ = std::move(callback);
}

void InputManager::processEvent(const SDL_Event& event) {
#ifdef TF_ENABLE_DEBUG_UI
    const ImGuiIO* imgui_io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
    const bool block_keyboard = imgui_io && imgui_io->WantCaptureKeyboard;
    const bool block_mouse = imgui_io && imgui_io->WantCaptureMouse;
#else
    const bool block_keyboard = false;
    const bool block_mouse = false;
#endif
    const auto* context_definition = currentContextDefinition();
    const auto* allowed_actions = context_definition != nullptr ? &context_definition->allowed_actions : nullptr;
    const Uint64 timestamp_ms = SDL_GetTicks();

    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (block_keyboard && event.key.down) {
                break;
            }
            if (event.key.down) {
                last_input_device_ = InputDevice::Keyboard;
            }
            handleInputEdge(event.key.scancode, key_to_actions_, key_down_states_, actions_, event.key.down, allowed_actions, timestamp_ms);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (block_mouse && event.button.down) {
                recalculateLogicalMousePosition();
                break;
            }
            if (event.button.down) {
                last_input_device_ = InputDevice::Mouse;
            }
            handleInputEdge(
                static_cast<Uint32>(event.button.button), mouse_to_actions_, mouse_down_states_, actions_, event.button.down, allowed_actions, timestamp_ms);
            mouse_position_ = {event.button.x, event.button.y};
            recalculateLogicalMousePosition();
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            last_input_device_ = InputDevice::Mouse;
            mouse_position_ = {event.motion.x, event.motion.y};
            recalculateLogicalMousePosition();
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (block_mouse) {
                break;
            }
            last_input_device_ = InputDevice::Mouse;
            mouse_wheel_delta_.x += event.wheel.x;
            mouse_wheel_delta_.y += event.wheel.y;
            break;
        case SDL_EVENT_GAMEPAD_ADDED: {
            const SDL_JoystickID instance_id = event.gdevice.which;
            connected_gamepad_ids_ = queryConnectedGamepadIds();
            switchActiveGamepad(instance_id);
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            const SDL_JoystickID instance_id = event.gdevice.which;
            connected_gamepad_ids_ = queryConnectedGamepadIds();
            if (instance_id == active_gamepad_id_) {
                clearGamepadContributions();
                closeActiveGamepad();
                if (!connected_gamepad_ids_.empty()) {
                    switchActiveGamepad(connected_gamepad_ids_.back());
                }
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMAPPED:
            if (event.gdevice.which == active_gamepad_id_) {
                spdlog::info("InputManager: 活动手柄 {} 映射已更新。", active_gamepad_id_);
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            if (event.gbutton.which != active_gamepad_id_) {
                break;
            }

            const auto button = static_cast<SDL_GamepadButton>(event.gbutton.button);
            if (button >= SDL_GAMEPAD_BUTTON_SOUTH && button < SDL_GAMEPAD_BUTTON_COUNT) {
                gamepad_button_states_[gamepadButtonIndex(button)] = event.gbutton.down;
            }
            if (event.gbutton.down) {
                last_input_device_ = InputDevice::Gamepad;
            }
            handleInputEdge(
                button, gamepad_button_to_actions_, gamepad_button_down_states_, actions_, event.gbutton.down, allowed_actions, timestamp_ms);
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            if (event.gaxis.which != active_gamepad_id_) {
                break;
            }

            const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
            if (axis < SDL_GAMEPAD_AXIS_LEFTX || axis >= SDL_GAMEPAD_AXIS_COUNT) {
                break;
            }

            const auto axis_index = gamepadAxisIndex(axis);
            gamepad_axis_raw_values_[axis_index] = event.gaxis.value;

            const bool is_trigger = (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            const float normalized_value = is_trigger
                ? normalizeTriggerAxis(event.gaxis.value)
                : normalizeStickAxis(event.gaxis.value);
            gamepad_axis_normalized_values_[axis_index] = normalized_value;

            const auto update_direction = [this, allowed_actions, timestamp_ms](GamepadAxisDirection direction, float magnitude) {
                const auto direction_index = gamepadAxisDirectionIndex(direction);
                const bool was_direction_down = gamepad_axis_direction_states_[direction_index];

                bool is_direction_down = was_direction_down;
                if (!was_direction_down && magnitude > AXIS_PRESS_THRESHOLD) {
                    is_direction_down = true;
                } else if (was_direction_down && magnitude < AXIS_RELEASE_THRESHOLD) {
                    is_direction_down = false;
                }

                gamepad_axis_direction_states_[direction_index] = is_direction_down;
                if (!was_direction_down && is_direction_down) {
                    last_input_device_ = InputDevice::Gamepad;
                }

                handleInputEdge(
                    direction, gamepad_axis_to_actions_, gamepad_axis_down_states_, actions_, is_direction_down, allowed_actions, timestamp_ms);
            };

            switch (axis) {
                case SDL_GAMEPAD_AXIS_LEFTX:
                    update_direction(GamepadAxisDirection::LeftStickLeft, std::max(-normalized_value, 0.0f));
                    update_direction(GamepadAxisDirection::LeftStickRight, std::max(normalized_value, 0.0f));
                    break;
                case SDL_GAMEPAD_AXIS_LEFTY:
                    update_direction(GamepadAxisDirection::LeftStickUp, std::max(-normalized_value, 0.0f));
                    update_direction(GamepadAxisDirection::LeftStickDown, std::max(normalized_value, 0.0f));
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTX:
                    update_direction(GamepadAxisDirection::RightStickLeft, std::max(-normalized_value, 0.0f));
                    update_direction(GamepadAxisDirection::RightStickRight, std::max(normalized_value, 0.0f));
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTY:
                    update_direction(GamepadAxisDirection::RightStickUp, std::max(-normalized_value, 0.0f));
                    update_direction(GamepadAxisDirection::RightStickDown, std::max(normalized_value, 0.0f));
                    break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                    update_direction(GamepadAxisDirection::LeftTrigger, normalized_value);
                    break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                    update_direction(GamepadAxisDirection::RightTrigger, normalized_value);
                    break;
                case SDL_GAMEPAD_AXIS_INVALID:
                case SDL_GAMEPAD_AXIS_COUNT:
                    break;
            }
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
            dispatcher_->trigger<engine::utils::WindowResizedEvent>(engine::utils::WindowResizedEvent{
                event.window.data1,
                event.window.data2,
                false
            });
            recalculateLogicalMousePosition();
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            dispatcher_->trigger<engine::utils::WindowResizedEvent>(engine::utils::WindowResizedEvent{
                event.window.data1,
                event.window.data2,
                true
            });
            recalculateLogicalMousePosition();
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MINIMIZED:
            clearAllInputState();
            dispatcher_->trigger<engine::utils::FocusLostEvent>();
            break;
        case SDL_EVENT_QUIT:
            quit();
            break;
        default:
            break;
    }
}

void InputManager::recalculateLogicalMousePosition() {
    if (!game_state_) {
        logical_mouse_position_ = mouse_position_;
        return;
    }

    const auto window_size = game_state_->getWindowSize();
    const auto logical_size = game_state_->getLogicalSize();
    const auto metrics = engine::utils::computeLetterboxMetrics(window_size, logical_size);
    if (metrics.scale <= 0.0f) {
        logical_mouse_position_ = mouse_position_;
        return;
    }

    const glm::vec2 local = mouse_position_ - metrics.viewport.pos;
    glm::vec2 logical = local / metrics.scale;

    logical.x = std::clamp(logical.x, 0.0f, logical_size.x);
    logical.y = std::clamp(logical.y, 0.0f, logical_size.y);
    logical_mouse_position_ = logical;
}

bool InputManager::loadConfig(std::string_view config_path) {
    auto mappings = loadInputMappingsConfig(config_path);
    if (!mappings.has_value()) {
        return false;
    }

    initializeMappings(*mappings);
    return true;
}

void InputManager::initializeMappings(const std::map<std::string, std::vector<std::string>>& actions_to_keyname) {
    spdlog::trace("初始化输入映射...");
    actions_.clear();
    action_bindings_.clear();
    action_dispatch_order_.clear();

    for (const auto& [action_name, key_names] : actions_to_keyname) {
        const auto action_name_id = entt::hashed_string(action_name.c_str());
        auto& entry = actions_[action_name_id];
        entry.name = action_name;
        action_dispatch_order_.push_back(action_name_id);
        auto& bindings = action_bindings_[action_name_id];
        bindings.clear();

        for (const auto& key_name : key_names) {
            const auto binding = bindingDefinitionFromToken(key_name);
            if (!binding.has_value()) {
                spdlog::warn("输入映射警告: 未知键或按钮名称 '{}' 用于动作 '{}'.", key_name, action_name);
                continue;
            }
            bindings.push_back(*binding);
        }
    }

    rebuildBindingCaches(false);
    initializeContextDefinitions();
    spdlog::trace("输入映射初始化完成.");
}

void InputManager::rebuildBindingCaches(bool reset_runtime_state) {
    if (reset_runtime_state) {
        clearAllInputState();
    }

    key_to_actions_.clear();
    mouse_to_actions_.clear();
    gamepad_button_to_actions_.clear();
    gamepad_axis_to_actions_.clear();
    rmlui_suppressed_navigation_scancodes_.clear();
    key_down_states_.clear();
    mouse_down_states_.clear();
    gamepad_button_down_states_.clear();
    gamepad_axis_down_states_.clear();

    for (auto action_id : action_dispatch_order_) {
        const auto action_it = actions_.find(action_id);
        if (action_it == actions_.end()) {
            continue;
        }

        const auto bindings_it = action_bindings_.find(action_id);
        if (bindings_it == action_bindings_.end()) {
            continue;
        }

        for (const auto& binding : bindings_it->second) {
            std::visit([&](const auto& value) {
                using ValueT = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<ValueT, SDL_Scancode>) {
                    key_to_actions_[value].push_back(action_id);
                    if (isMenuNavigationActionName(action_it->second.name) && value != SDL_SCANCODE_TAB) {
                        rmlui_suppressed_navigation_scancodes_.insert(value);
                    }
                } else if constexpr (std::is_same_v<ValueT, Uint32>) {
                    mouse_to_actions_[value].push_back(action_id);
                } else if constexpr (std::is_same_v<ValueT, SDL_GamepadButton>) {
                    gamepad_button_to_actions_[value].push_back(action_id);
                } else {
                    gamepad_axis_to_actions_[value].push_back(action_id);
                }
            }, binding.physical_input);
        }
    }
}

void InputManager::initializeContextDefinitions() {
    context_definitions_ = buildInputContextDefinitions(action_dispatch_order_, actions_);
}

void InputManager::initializeConnectedGamepads() {
    connected_gamepad_ids_ = queryConnectedGamepadIds();
    if (!connected_gamepad_ids_.empty()) {
        switchActiveGamepad(connected_gamepad_ids_.back());
    }
}

void InputManager::switchActiveGamepad(SDL_JoystickID instance_id) {
    if (instance_id == 0) {
        return;
    }
    if (active_gamepad_ != nullptr && instance_id == active_gamepad_id_) {
        return;
    }
    if (!SDL_IsGamepad(instance_id)) {
        spdlog::warn("InputManager: 设备 {} 不是 SDL gamepad。", instance_id);
        return;
    }

    if (active_gamepad_ != nullptr) {
        clearGamepadContributions();
        closeActiveGamepad();
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(instance_id);
    if (gamepad == nullptr) {
        spdlog::warn("InputManager: 打开手柄 {} 失败: {}", instance_id, SDL_GetError());
        return;
    }

    active_gamepad_ = gamepad;
    active_gamepad_id_ = instance_id;
}

void InputManager::closeActiveGamepad() {
    if (active_gamepad_ != nullptr) {
        SDL_CloseGamepad(active_gamepad_);
        active_gamepad_ = nullptr;
    }
    active_gamepad_id_ = 0;
}

void InputManager::clearGamepadContributions() {
    const auto decrement_action = [this](entt::id_type action_id) {
        auto action_it = actions_.find(action_id);
        if (action_it == actions_.end()) {
            return;
        }

        auto& entry = action_it->second;
        if (entry.active_count > 0) {
            --entry.active_count;
        }
        if (entry.active_count == 0) {
            entry.state = ActionState::INACTIVE;
        }
    };

    for (const auto& [button, is_down] : gamepad_button_down_states_) {
        if (!is_down) {
            continue;
        }
        if (auto mapping_it = gamepad_button_to_actions_.find(button); mapping_it != gamepad_button_to_actions_.end()) {
            for (auto action_id : mapping_it->second) {
                decrement_action(action_id);
            }
        }
    }

    for (const auto& [direction, is_down] : gamepad_axis_down_states_) {
        if (!is_down) {
            continue;
        }
        if (auto mapping_it = gamepad_axis_to_actions_.find(direction); mapping_it != gamepad_axis_to_actions_.end()) {
            for (auto action_id : mapping_it->second) {
                decrement_action(action_id);
            }
        }
    }

    for (auto& [_, is_down] : gamepad_button_down_states_) {
        is_down = false;
    }
    for (auto& [_, is_down] : gamepad_axis_down_states_) {
        is_down = false;
    }
    resetGamepadDebugState();
}

void InputManager::clearAllInputState() {
    for (auto& [_, down] : key_down_states_) {
        down = false;
    }
    for (auto& [_, down] : mouse_down_states_) {
        down = false;
    }
    for (auto& [_, down] : gamepad_button_down_states_) {
        down = false;
    }
    for (auto& [_, down] : gamepad_axis_down_states_) {
        down = false;
    }

    resetGamepadDebugState();
    // Context 切换不应把一次性的滚轮输入带到下一个 scene / context。
    mouse_wheel_delta_ = {0.0f, 0.0f};
    for (auto& [_, entry] : actions_) {
        entry.active_count = 0;
        entry.state = ActionState::INACTIVE;
        entry.press_buffer.clear();
    }
}

bool InputManager::persistBindings() const {
    return persistInputBindingsConfig(config_path_, action_dispatch_order_, actions_, action_bindings_);
}

bool InputManager::applyBindingReplacement(entt::id_type action_name_id,
                                           std::size_t binding_index,
                                           const BindingDefinition& replacement,
                                           const std::vector<std::pair<entt::id_type, std::size_t>>& conflicts) {
    const auto backup = action_bindings_;
    auto& bindings = action_bindings_[action_name_id];
    if (binding_index > bindings.size()) {
        return false;
    }

    if (binding_index == bindings.size()) {
        bindings.push_back(replacement);
    } else {
        bindings[binding_index] = replacement;
    }

    std::unordered_map<entt::id_type, std::vector<std::size_t>> conflict_map;
    for (const auto& [conflict_action_id, conflict_index] : conflicts) {
        conflict_map[conflict_action_id].push_back(conflict_index);
    }

    for (auto& [conflict_action_id, indexes] : conflict_map) {
        auto conflict_bindings_it = action_bindings_.find(conflict_action_id);
        if (conflict_bindings_it == action_bindings_.end()) {
            continue;
        }

        std::sort(indexes.begin(), indexes.end(), std::greater<>{});
        indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
        for (const auto index : indexes) {
            if (index < conflict_bindings_it->second.size()) {
                conflict_bindings_it->second.erase(conflict_bindings_it->second.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }
    }

    rebuildBindingCaches(true);
    if (!persistBindings()) {
        action_bindings_ = backup;
        rebuildBindingCaches(true);
        return false;
    }

    return true;
}

std::vector<std::pair<entt::id_type, std::size_t>> InputManager::findBindingConflicts(
    entt::id_type action_name_id,
    std::size_t binding_index,
    const BindingDefinition& candidate) const {
    std::vector<std::pair<entt::id_type, std::size_t>> conflicts;

    for (const auto& [existing_action_id, bindings] : action_bindings_) {
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            if (existing_action_id == action_name_id && i == binding_index) {
                continue;
            }
            if (bindings[i].physical_input == candidate.physical_input) {
                conflicts.emplace_back(existing_action_id, i);
            }
        }
    }

    return conflicts;
}

bool InputManager::handleRebindCaptureEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            // Capture 模式会阻断 action dispatch，因此保留 Escape 作为固定的物理取消键。
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                cancelRebindCapture();
                return true;
            }
            [[fallthrough]];
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const auto candidate = bindingDefinitionFromEvent(event, active_gamepad_id_, AXIS_PRESS_THRESHOLD);
            if (!candidate.has_value()) {
                return true;
            }

            const auto conflicts = findBindingConflicts(rebind_capture_.action_id, rebind_capture_.binding_index, *candidate);
            const auto captured_action_id = rebind_capture_.action_id;
            const auto captured_binding_index = rebind_capture_.binding_index;
            rebind_capture_ = RebindCaptureState{};
            if (!conflicts.empty()) {
                pending_rebind_conflict_ = PendingRebindConflict{
                    .action_id = captured_action_id,
                    .binding_index = captured_binding_index,
                    .replacement = *candidate,
                    .conflicts = conflicts,
                };
                return true;
            }

            (void)applyBindingReplacement(captured_action_id, captured_binding_index, *candidate, {});
            return true;
        }
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_TEXT_EDITING:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            return true;
        default:
            return false;
    }
}

const InputContextDefinition* InputManager::currentContextDefinition() const {
    if (context_stack_.empty()) {
        return nullptr;
    }

    if (auto it = context_definitions_.find(context_stack_.back()); it != context_definitions_.end()) {
        return &it->second;
    }

    return nullptr;
}

void InputManager::resetGamepadDebugState() {
    gamepad_button_states_.fill(false);
    gamepad_axis_direction_states_.fill(false);
    gamepad_axis_raw_values_.fill(0);
    gamepad_axis_normalized_values_.fill(0.0f);
}

void InputManager::setActionStateDebug(entt::id_type action_name_id, ActionState state) {
    auto it = actions_.find(action_name_id);
    if (it == actions_.end()) {
        spdlog::warn("尝试设置未注册的动作状态: {}", action_name_id);
        return;
    }
    it->second.state = state;
    if (state == ActionState::PRESSED) {
        it->second.press_buffer.push(SDL_GetTicks());
    } else if (state == ActionState::INACTIVE) {
        it->second.press_buffer.clear();
    }
    spdlog::trace("调试: 手动设置动作状态 {} 为 {}", action_name_id, static_cast<int>(state));
}

} // namespace engine::input
