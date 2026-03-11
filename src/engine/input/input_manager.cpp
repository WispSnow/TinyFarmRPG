#include "engine/input/input_manager.h"

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
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <utility>

namespace engine::input {

namespace {

constexpr float AXIS_PRESS_THRESHOLD = 0.6f;
constexpr float AXIS_RELEASE_THRESHOLD = 0.4f;

[[nodiscard]] std::map<std::string, std::vector<std::string>> defaultMappings() {
    return {
        {"move_left", {"A", "Left", "GamepadDpadLeft", "LeftStickLeft"}},
        {"move_right", {"D", "Right", "GamepadDpadRight", "LeftStickRight"}},
        {"move_up", {"W", "Up", "GamepadDpadUp", "LeftStickUp"}},
        {"move_down", {"S", "Down", "GamepadDpadDown", "LeftStickDown"}},
        {"primary_action", {"MouseLeft", "GamepadSouth"}},
        {"secondary_action", {"MouseRight", "GamepadEast"}},
        {"pause", {"P", "Escape", "GamepadStart"}},
        {"interact", {"F", "GamepadWest"}},
        {"inventory", {"I", "GamepadBack"}},
        {"hotbar", {"Tab", "GamepadNorth"}},
        {"hotbar_prev", {"GamepadLeftShoulder"}},
        {"hotbar_next", {"GamepadRightShoulder"}},
        {"rotate_left", {"Q"}},
        {"rotate_right", {"E"}},
        {"player_light", {"L"}},
        {"camera_reset_zoom", {"MouseMiddle"}},
        {"hotbar_1", {"1"}},
        {"hotbar_2", {"2"}},
        {"hotbar_3", {"3"}},
        {"hotbar_4", {"4"}},
        {"hotbar_5", {"5"}},
        {"hotbar_6", {"6"}},
        {"hotbar_7", {"7"}},
        {"hotbar_8", {"8"}},
        {"hotbar_9", {"9"}},
        {"hotbar_10", {"0"}},
    };
}

template <typename KeyT>
void handleInputEdge(KeyT key,
                     std::unordered_map<KeyT, std::vector<entt::id_type>>& mapping,
                     std::unordered_map<KeyT, bool>& down_states,
                     std::unordered_map<entt::id_type, ActionEntry>& actions,
                     bool is_down,
                     const std::unordered_set<entt::id_type>* allowed_actions) {
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

[[nodiscard]] float normalizeStickAxis(Sint16 value) {
    const float denominator = value < 0 ? 32768.0f : 32767.0f;
    return std::clamp(static_cast<float>(value) / denominator, -1.0f, 1.0f);
}

[[nodiscard]] float normalizeTriggerAxis(Sint16 value) {
    if (value <= 0) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
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
    : dispatcher_(dispatcher), game_state_(game_state) {
    if (!loadConfig(config_path)) {
        initializeMappings(defaultMappings());
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
        bool imgui_blocks_rmlui = false;
#ifdef TF_ENABLE_DEBUG_UI
        if (imgui_event_callback_) {
            imgui_event_callback_(event);
        }
        imgui_blocks_rmlui = isImGuiBlockingRmlUi(event);
#endif

        bool should_propagate = true;
        if (!imgui_blocks_rmlui && rmlui_event_callback_) {
            should_propagate = rmlui_event_callback_(event);
        }

        // Phase 1 有意只强制放行清理型/生命周期型手柄事件；
        // GAMEPAD_BUTTON_DOWN 仍走现有 UI 路由，后续 Phase 4 再引入手柄 UI 导航消费策略。
        const bool always_propagate =
            (event.type == SDL_EVENT_KEY_UP
             || event.type == SDL_EVENT_MOUSE_BUTTON_UP
             || event.type == SDL_EVENT_MOUSE_MOTION
             || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP
             || event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION
             || event.type == SDL_EVENT_GAMEPAD_ADDED
             || event.type == SDL_EVENT_GAMEPAD_REMOVED
             || event.type == SDL_EVENT_GAMEPAD_REMAPPED);

        if (!should_propagate && !always_propagate) {
            continue;
        }

        processEvent(event);
    }
}

void InputManager::dispatchActionCallbacks() {
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

void InputManager::setRmlUiEventForwarder(std::function<bool(SDL_Event&)> callback) {
    rmlui_event_callback_ = std::move(callback);
}

void InputManager::setImGuiEventForwarder(std::function<void(const SDL_Event&)> callback) {
#ifdef TF_ENABLE_DEBUG_UI
    imgui_event_callback_ = std::move(callback);
#else
    (void)callback;
#endif
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

    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (block_keyboard && event.key.down) {
                break;
            }
            if (event.key.down) {
                last_input_device_ = InputDevice::KeyboardMouse;
            }
            handleInputEdge(event.key.scancode, key_to_actions_, key_down_states_, actions_, event.key.down, allowed_actions);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (block_mouse && event.button.down) {
                recalculateLogicalMousePosition();
                break;
            }
            if (event.button.down) {
                last_input_device_ = InputDevice::KeyboardMouse;
            }
            handleInputEdge(
                static_cast<Uint32>(event.button.button), mouse_to_actions_, mouse_down_states_, actions_, event.button.down, allowed_actions);
            mouse_position_ = {event.button.x, event.button.y};
            recalculateLogicalMousePosition();
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            last_input_device_ = InputDevice::KeyboardMouse;
            mouse_position_ = {event.motion.x, event.motion.y};
            recalculateLogicalMousePosition();
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (block_mouse) {
                break;
            }
            last_input_device_ = InputDevice::KeyboardMouse;
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
                button, gamepad_button_to_actions_, gamepad_button_down_states_, actions_, event.gbutton.down, allowed_actions);
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

            const auto update_direction = [this, allowed_actions](GamepadAxisDirection direction, float magnitude) {
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
                    direction, gamepad_axis_to_actions_, gamepad_axis_down_states_, actions_, is_direction_down, allowed_actions);
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
    if (config_path.empty()) {
        return false;
    }

    const std::filesystem::path path{config_path};
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("InputManager: 无法打开输入配置文件 '{}'，使用默认映射。", path.string());
        return false;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json json = nlohmann::json::parse(file_content, nullptr, false);
    if (json.is_discarded()) {
        spdlog::warn("InputManager: 解析输入配置 '{}' 失败，使用默认映射。", path.string());
        return false;
    }

    const nlohmann::json* mappings_node = &json;
    if (auto it = json.find("input_mappings"); it != json.end()) {
        mappings_node = &(*it);
    }

    if (!mappings_node->is_object()) {
        spdlog::warn("InputManager: 输入配置文件 '{}' 缺少 input_mappings 对象，使用默认映射。", path.string());
        return false;
    }

    std::map<std::string, std::vector<std::string>> mappings;
    for (const auto& [action_name, key_array] : mappings_node->items()) {
        if (!key_array.is_array()) {
            spdlog::warn("InputManager: 输入配置文件 '{}' 的映射 '{}' 不是数组，使用默认映射。", path.string(), action_name);
            return false;
        }

        std::vector<std::string> key_names;
        key_names.reserve(key_array.size());
        for (const auto& item : key_array) {
            if (!item.is_string()) {
                spdlog::warn("InputManager: 输入配置文件 '{}' 的映射 '{}' 包含非字符串条目，使用默认映射。", path.string(), action_name);
                return false;
            }
            key_names.push_back(item.get<std::string>());
        }

        mappings.emplace(action_name, std::move(key_names));
    }

    initializeMappings(mappings);
    spdlog::info("InputManager: 成功加载输入配置 '{}'", path.string());
    return true;
}

void InputManager::initializeMappings(const std::map<std::string, std::vector<std::string>>& actions_to_keyname) {
    spdlog::trace("初始化输入映射...");
    key_to_actions_.clear();
    mouse_to_actions_.clear();
    gamepad_button_to_actions_.clear();
    gamepad_axis_to_actions_.clear();
    key_down_states_.clear();
    mouse_down_states_.clear();
    gamepad_button_down_states_.clear();
    gamepad_axis_down_states_.clear();
    actions_.clear();
    action_dispatch_order_.clear();

    for (const auto& [action_name, key_names] : actions_to_keyname) {
        const auto action_name_id = entt::hashed_string(action_name.c_str());
        auto& entry = actions_[action_name_id];
        entry.name = action_name;
        action_dispatch_order_.push_back(action_name_id);

        for (const auto& key_name : key_names) {
            const SDL_Scancode scancode = scancodeFromString(key_name);
            const Uint32 mouse_button = mouseButtonFromString(key_name);
            const SDL_GamepadButton gamepad_button = gamepadButtonFromString(key_name);
            const auto gamepad_axis_direction = gamepadAxisDirectionFromString(key_name);

            if (scancode != SDL_SCANCODE_UNKNOWN) {
                key_to_actions_[scancode].push_back(action_name_id);
            } else if (mouse_button != 0) {
                mouse_to_actions_[mouse_button].push_back(action_name_id);
            } else if (gamepad_button != SDL_GAMEPAD_BUTTON_INVALID) {
                gamepad_button_to_actions_[gamepad_button].push_back(action_name_id);
            } else if (gamepad_axis_direction.has_value()) {
                gamepad_axis_to_actions_[*gamepad_axis_direction].push_back(action_name_id);
            } else {
                spdlog::warn("输入映射警告: 未知键或按钮名称 '{}' 用于动作 '{}'.", key_name, action_name);
            }
        }
    }

    initializeContextDefinitions();
    spdlog::trace("输入映射初始化完成.");
}

void InputManager::initializeContextDefinitions() {
    context_definitions_.clear();

    const auto build_definition = [this](std::initializer_list<const char*> action_names) {
        InputContextDefinition definition;

        for (const char* action_name : action_names) {
            const auto action_id = entt::hashed_string{action_name}.value();
            if (actions_.contains(action_id)) {
                definition.allowed_actions.insert(action_id);
            }
        }

        definition.dispatch_actions.reserve(definition.allowed_actions.size());
        for (auto action_id : action_dispatch_order_) {
            if (definition.allowed_actions.contains(action_id)) {
                definition.dispatch_actions.push_back(action_id);
            }
        }

        return definition;
    };

    context_definitions_.emplace(
        InputContextId::Gameplay,
        build_definition({
            "move_left",
            "move_right",
            "move_up",
            "move_down",
            "primary_action",
            "secondary_action",
            "interact",
            "pause",
            "inventory",
            "hotbar",
            "hotbar_prev",
            "hotbar_next",
            "hotbar_1",
            "hotbar_2",
            "hotbar_3",
            "hotbar_4",
            "hotbar_5",
            "hotbar_6",
            "hotbar_7",
            "hotbar_8",
            "hotbar_9",
            "hotbar_10",
            "rotate_left",
            "rotate_right",
            "player_light",
            "camera_reset_zoom",
        }));
    context_definitions_.emplace(InputContextId::Menu, build_definition({"pause"}));
    context_definitions_.emplace(InputContextId::Dialogue, build_definition({}));
    context_definitions_.emplace(InputContextId::Battle, build_definition({}));
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

SDL_Scancode InputManager::scancodeFromString(std::string_view key_name) const {
    return SDL_GetScancodeFromName(std::string(key_name).c_str());
}

Uint32 InputManager::mouseButtonFromString(std::string_view button_name) const {
    if (button_name == "MouseLeft") return SDL_BUTTON_LEFT;
    if (button_name == "MouseMiddle") return SDL_BUTTON_MIDDLE;
    if (button_name == "MouseRight") return SDL_BUTTON_RIGHT;
    if (button_name == "MouseX1") return SDL_BUTTON_X1;
    if (button_name == "MouseX2") return SDL_BUTTON_X2;
    return 0;
}

SDL_GamepadButton InputManager::gamepadButtonFromString(std::string_view button_name) const {
    if (button_name == "GamepadSouth") return SDL_GAMEPAD_BUTTON_SOUTH;
    if (button_name == "GamepadEast") return SDL_GAMEPAD_BUTTON_EAST;
    if (button_name == "GamepadWest") return SDL_GAMEPAD_BUTTON_WEST;
    if (button_name == "GamepadNorth") return SDL_GAMEPAD_BUTTON_NORTH;
    if (button_name == "GamepadBack") return SDL_GAMEPAD_BUTTON_BACK;
    if (button_name == "GamepadGuide") return SDL_GAMEPAD_BUTTON_GUIDE;
    if (button_name == "GamepadStart") return SDL_GAMEPAD_BUTTON_START;
    if (button_name == "GamepadLeftStick") return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    if (button_name == "GamepadRightStick") return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    if (button_name == "GamepadLeftShoulder") return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (button_name == "GamepadRightShoulder") return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    if (button_name == "GamepadDpadUp") return SDL_GAMEPAD_BUTTON_DPAD_UP;
    if (button_name == "GamepadDpadDown") return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    if (button_name == "GamepadDpadLeft") return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    if (button_name == "GamepadDpadRight") return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    return SDL_GAMEPAD_BUTTON_INVALID;
}

std::optional<GamepadAxisDirection> InputManager::gamepadAxisDirectionFromString(std::string_view axis_name) const {
    if (axis_name == "LeftStickUp") return GamepadAxisDirection::LeftStickUp;
    if (axis_name == "LeftStickDown") return GamepadAxisDirection::LeftStickDown;
    if (axis_name == "LeftStickLeft") return GamepadAxisDirection::LeftStickLeft;
    if (axis_name == "LeftStickRight") return GamepadAxisDirection::LeftStickRight;
    if (axis_name == "RightStickUp") return GamepadAxisDirection::RightStickUp;
    if (axis_name == "RightStickDown") return GamepadAxisDirection::RightStickDown;
    if (axis_name == "RightStickLeft") return GamepadAxisDirection::RightStickLeft;
    if (axis_name == "RightStickRight") return GamepadAxisDirection::RightStickRight;
    if (axis_name == "LeftTrigger") return GamepadAxisDirection::LeftTrigger;
    if (axis_name == "RightTrigger") return GamepadAxisDirection::RightTrigger;
    return std::nullopt;
}

void InputManager::setActionStateDebug(entt::id_type action_name_id, ActionState state) {
    auto it = actions_.find(action_name_id);
    if (it == actions_.end()) {
        spdlog::warn("尝试设置未注册的动作状态: {}", action_name_id);
        return;
    }
    it->second.state = state;
    spdlog::trace("调试: 手动设置动作状态 {} 为 {}", action_name_id, static_cast<int>(state));
}

} // namespace engine::input
