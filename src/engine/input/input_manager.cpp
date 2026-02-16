#include "engine/input/input_manager.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"
#include "engine/utils/math.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>
#include <utility>
#include <entt/signal/dispatcher.hpp>
#include <entt/core/hashed_string.hpp>
#ifdef TF_ENABLE_DEBUG_UI
#include <imgui.h>
#endif
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>


namespace engine::input {

namespace {

[[nodiscard]] std::unordered_map<std::string, std::vector<std::string>> defaultMappings() {
    return {
        {"move_left", {"A", "Left"}},
        {"move_right", {"D", "Right"}},
        {"move_up", {"W", "Up"}},
        {"move_down", {"S", "Down"}},
        {"jump", {"J", "Space"}},
        {"attack", {"K", "MouseLeft"}},
        {"pause", {"P", "Escape"}},
    };
}

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
    // 获取初始鼠标位置
    float x, y;
    SDL_GetMouseState(&x, &y);
    mouse_position_ = {x, y};
    recalculateLogicalMousePosition();
    spdlog::trace("初始鼠标位置: ({}, {})", mouse_position_.x, mouse_position_.y);
}

entt::sink<entt::sigh<bool()>> InputManager::onAction(entt::id_type action_name_id, ActionState action_state) {
    // NOTE: INACTIVE 不是“可绑定”的回调阶段，仅用于运行时状态。
    // 为了避免 .at() 越界导致的异常/终止，这里做显式防御并回退到 PRESSED。
    if (action_state == ActionState::INACTIVE) {
        spdlog::warn("InputManager::onAction: ActionState::INACTIVE 不能用于绑定回调，将回退到 PRESSED。");
        action_state = ActionState::PRESSED;
    }

    const auto index = static_cast<std::size_t>(action_state);
    if (index >= CALLBACK_STATE_COUNT) {
        spdlog::warn("InputManager::onAction: 非法 ActionState={}，将回退到 PRESSED。", index);
        return actions_to_func_[action_name_id][0];
    }

    // 如果 action_name 不存在，自动创建一条回调数组（懒加载）
    return actions_to_func_[action_name_id][index];
}

// --- 更新和事件处理 ---

void InputManager::update() {
    // 1. 根据上一帧的值更新默认的动作状态
    for (auto& [action_name_id, state] : action_states_) {
        if (state == ActionState::PRESSED) {
            state = ActionState::HELD;                 // 当某个键按下不动时，并不会生成SDL_Event。
        } else if (state == ActionState::RELEASED) {
            state = ActionState::INACTIVE;
        }
    }

    mouse_wheel_delta_ = {0.0f, 0.0f};  // 滚轮数据每帧重置

    // 2. 处理所有待处理的 SDL 事件 (这将设定 action_states_ 的值)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#ifdef TF_ENABLE_DEBUG_UI
        if (imgui_event_callback_) {
            imgui_event_callback_(event);
        }
#endif
        processEvent(event);
    }

    // 3. 触发回调
    for (auto& [action_name_id, state] : action_states_) {
        if (state != ActionState::INACTIVE) {   // 如果动作状态不是 INACTIVE，
            // 且有绑定回调函数
            if (auto it = actions_to_func_.find(action_name_id); it != actions_to_func_.end()) {
                // collect方法可以获取回调函数返回值，放入lambda函数的参数中。
                // 而lambda函数的返回值为真时，停止分发信号。
                // 分发信号的顺序为“后绑定先调用”
                it->second.at(static_cast<size_t>(state)).collect([](bool result) {
                    return result;
                });
            }
        }
    }
}

void InputManager::quit() {
    dispatcher_->trigger<engine::utils::QuitEvent>();
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

    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            SDL_Scancode scancode = event.key.scancode;     // 获取按键的scancode
            bool is_down = event.key.down; 
            bool is_repeat = event.key.repeat;

            if (block_keyboard && is_down) {
                break;
            }

            auto it = input_to_actions_.find(scancode);
            if (it != input_to_actions_.end()) {     // 如果按键有对应的action
                const std::vector<entt::id_type>& associated_actions = it->second;
                for (const auto& action_name : associated_actions) {
                    updateActionState(action_name, is_down, is_repeat); // 更新action状态
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (block_mouse && event.button.down) {
                recalculateLogicalMousePosition();
                break;
            }
            Uint32 button = event.button.button;              // 获取鼠标按钮
            bool is_down = event.button.down;
            auto it = input_to_actions_.find(button);
            if (it != input_to_actions_.end()) {     // 如果鼠标按钮有对应的action
                const std::vector<entt::id_type>& associated_actions = it->second;
                for (const auto& action_name : associated_actions) {
                    // 鼠标事件不考虑repeat, 所以第三个参数传false
                    updateActionState(action_name, is_down, false); // 更新action状态
                }
            }
            // 在点击时更新鼠标位置，同时更新逻辑位置
            mouse_position_ = {event.button.x, event.button.y};
            recalculateLogicalMousePosition();
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:        // 处理鼠标运动
            mouse_position_ = {event.motion.x, event.motion.y};
            recalculateLogicalMousePosition();
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            if (block_mouse) {
                break;
            }
            mouse_wheel_delta_ = {event.wheel.x, event.wheel.y};
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            // SDL3: 窗口大小改变（逻辑坐标）
            dispatcher_->trigger<engine::utils::WindowResizedEvent>(engine::utils::WindowResizedEvent{
                event.window.data1,
                event.window.data2,
                false
            });
            recalculateLogicalMousePosition();
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            // 高DPI：像素大小改变
            dispatcher_->trigger<engine::utils::WindowResizedEvent>(engine::utils::WindowResizedEvent{
                event.window.data1,
                event.window.data2,
                true
            });
            recalculateLogicalMousePosition();
            break;
        }
        case SDL_EVENT_QUIT:
            quit();
            break;
        default:
            break;
    }
}

// --- 状态查询方法 ---

bool InputManager::isActionDown(entt::id_type action_name_id) const {
    // C++17 引入的 “带有初始化语句的 if 语句”
    if (auto it = action_states_.find(action_name_id); it != action_states_.end()) {
        return it->second == ActionState::PRESSED || it->second == ActionState::HELD;
    }
    return false;
}

bool InputManager::isActionPressed(entt::id_type action_name_id) const {
    if (auto it = action_states_.find(action_name_id); it != action_states_.end()) {
        return it->second == ActionState::PRESSED;
    }
    return false;
}

bool InputManager::isActionReleased(entt::id_type action_name_id) const {
    if (auto it = action_states_.find(action_name_id); it != action_states_.end()) {
        return it->second == ActionState::RELEASED;
    }
    return false;
}

glm::vec2 InputManager::getMousePosition() const
{
    return mouse_position_;
}

void InputManager::recalculateLogicalMousePosition()
{
    if (!game_state_) {
        logical_mouse_position_ = mouse_position_;
        return;
    }

    // SDL 的鼠标坐标（event.motion.x/y、SDL_GetMouseState）以“window coordinates”为单位，
    // 对应 SDL_GetWindowSize 返回的窗口坐标尺寸；高 DPI 下该尺寸可能不同于实际像素尺寸。
    // 渲染侧（OpenGL viewport）使用 SDL_GetWindowSizeInPixels，因此这里必须用 window size
    // 来做 letterbox 映射，才能和鼠标输入坐标保持同一坐标系。
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

glm::vec2 InputManager::getLogicalMousePosition() const
{
    // 每帧最多计算一次，避免每次调用时计算
    return logical_mouse_position_;
}

glm::vec2 InputManager::getMouseWheelDelta() const
{
    return mouse_wheel_delta_;
}

void InputManager::setImGuiEventForwarder(std::function<void(const SDL_Event&)> callback) {
#ifdef TF_ENABLE_DEBUG_UI
    imgui_event_callback_ = std::move(callback);
#else
    (void)callback;
#endif
}

// --- 初始化输入映射 ---

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

    std::unordered_map<std::string, std::vector<std::string>> mappings;
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

void InputManager::initializeMappings(const std::unordered_map<std::string, std::vector<std::string>>& actions_to_keyname) {
    spdlog::trace("初始化输入映射...");
    auto resolved_mappings = actions_to_keyname;
    input_to_actions_.clear();
    action_states_.clear();
    action_id_to_name_.clear();

    // 如果配置中没有定义鼠标按钮动作(通常不需要配置),则添加默认映射, 用于 UI
    if (resolved_mappings.find("mouse_left") == resolved_mappings.end()) {
        spdlog::debug("配置中没有定义 'mouse_left' 动作,添加默认映射到 'MouseLeft'.");
        resolved_mappings["mouse_left"] = {"MouseLeft"};
    }
    if (resolved_mappings.find("mouse_right") == resolved_mappings.end()) {
        spdlog::debug("配置中没有定义 'mouse_right' 动作,添加默认映射到 'MouseRight'.");
        resolved_mappings["mouse_right"] = {"MouseRight"};
    }
    // 遍历 动作 -> 按键名称 的映射
    for (const auto& [action_name, key_names] : resolved_mappings) {
        // 每个动作对应一个动作状态，初始化为 INACTIVE
        auto action_name_id = entt::hashed_string(action_name.c_str());
        action_states_[action_name_id] = ActionState::INACTIVE;
        action_id_to_name_[action_name_id] = action_name;
        spdlog::trace("映射动作: {}", action_name);
        // 设置 "按键 -> 动作" 的映射
        for (const auto& key_name : key_names) {
            SDL_Scancode scancode = scancodeFromString(key_name);       // 尝试根据按键名称获取scancode
            Uint32 mouse_button = mouseButtonFromString(key_name);  // 尝试根据按键名称获取鼠标按钮
            // 未来可添加其它输入类型 ...

            if (scancode != SDL_SCANCODE_UNKNOWN) {      // 如果scancode有效,则将action添加到scancode_to_actions_map_中
                input_to_actions_[scancode].push_back(action_name_id);     
                spdlog::trace("  映射按键: {} (Scancode: {}) 到动作: {}", key_name, static_cast<int>(scancode), action_name);
            } else if (mouse_button != 0) {             // 如果鼠标按钮有效,则将action添加到mouse_button_to_actions_map_中
                input_to_actions_[mouse_button].push_back(action_name_id); 
                spdlog::trace("  映射鼠标按钮: {} (Button ID: {}) 到动作: {}", key_name, static_cast<int>(mouse_button), action_name);
                // else if: 未来可添加其它输入类型 ...
            } else {
                spdlog::warn("输入映射警告: 未知键或按钮名称 '{}' 用于动作 '{}'.", key_name, action_name);
            }
        }
    }
    spdlog::trace("输入映射初始化完成.");
}

// --- 工具函数 ---
// 将字符串名称转换为 SDL_Scancode
SDL_Scancode InputManager::scancodeFromString(std::string_view key_name) {
    return SDL_GetScancodeFromName(key_name.data());
}

// 将鼠标按钮名称字符串转换为 SDL 按钮 Uint32 值
Uint32 InputManager::mouseButtonFromString(std::string_view button_name) {
    if (button_name == "MouseLeft") return SDL_BUTTON_LEFT;
    if (button_name == "MouseMiddle") return SDL_BUTTON_MIDDLE;
    if (button_name == "MouseRight") return SDL_BUTTON_RIGHT;
    // SDL 还定义了 SDL_BUTTON_X1 和 SDL_BUTTON_X2
    if (button_name == "MouseX1") return SDL_BUTTON_X1;
    if (button_name == "MouseX2") return SDL_BUTTON_X2;
    return 0; // 0 不是有效的按钮值，表示无效
}

void InputManager::updateActionState(entt::id_type action_name_id, bool is_input_active, bool is_repeat_event) {
    auto it = action_states_.find(action_name_id);
    if (it == action_states_.end()) {
        spdlog::warn("尝试更新未注册的动作状态: {}", action_name_id);
        return;
    }

    if (is_input_active) { // 输入被激活 (按下)
        if (is_repeat_event) {
            it->second = ActionState::HELD; 
        } else {            // 非重复的按下事件
            it->second = ActionState::PRESSED;
        }
    } else { // 输入被释放 (松开)
        // 避免“从未按下却收到松开事件”导致的伪 RELEASED（例如 UI capture 或焦点切换）
        if (it->second != ActionState::INACTIVE) {
            it->second = ActionState::RELEASED;
        }
    }
}

void InputManager::setActionStateDebug(entt::id_type action_name_id, ActionState state) {
    auto it = action_states_.find(action_name_id);
    if (it == action_states_.end()) {
        spdlog::warn("尝试设置未注册的动作状态: {}", action_name_id);
        return;
    }
    it->second = state;
    spdlog::trace("调试: 手动设置动作状态 {} 为 {}", action_name_id, static_cast<int>(state));
}

} // namespace engine::input 
