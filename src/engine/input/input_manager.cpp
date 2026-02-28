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

[[nodiscard]] std::map<std::string, std::vector<std::string>> defaultMappings() {
    // 与 config/input.json 保持同步，作为配置加载失败时的后备映射
    return {
        {"move_left", {"A", "Left"}},
        {"move_right", {"D", "Right"}},
        {"move_up", {"W", "Up"}},
        {"move_down", {"S", "Down"}},
        {"pause", {"P", "Escape"}},
        {"interact", {"F"}},
        {"inventory", {"I"}},
        {"hotbar", {"Tab"}},
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

/// @brief 处理物理输入的按下/释放边沿，更新关联动作的 active_count 和状态
template <typename KeyT>
void handleInputEdge(KeyT key,
                     std::unordered_map<KeyT, std::vector<entt::id_type>>& mapping,
                     std::unordered_map<KeyT, bool>& down_states,
                     std::unordered_map<entt::id_type, ActionEntry>& actions,
                     bool is_down) {
    auto actions_it = mapping.find(key);
    if (actions_it == mapping.end()) return;

    auto& was_down = down_states[key];
    if (is_down == was_down) return;
    was_down = is_down;

    for (auto action_id : actions_it->second) {
        auto& entry = actions[action_id];
        if (is_down) {
            entry.active_count++;
            if (entry.active_count == 1) {
                entry.state = ActionState::PRESSED;
            }
        } else {
            if (entry.active_count > 0) {
                entry.active_count--;
                if (entry.active_count == 0) {
                    entry.state = ActionState::RELEASED;
                }
            }
        }
    }
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
    // 获取初始鼠标位置
    float x, y;
    SDL_GetMouseState(&x, &y);
    mouse_position_ = {x, y};
    recalculateLogicalMousePosition();
    spdlog::trace("初始鼠标位置: ({}, {})", mouse_position_.x, mouse_position_.y);
}

entt::sink<entt::sigh<bool()>> InputManager::onAction(entt::id_type action_name_id, ActionState action_state) {
    // NOTE: INACTIVE 不是"可绑定"的回调阶段，仅用于运行时状态。
    if (action_state == ActionState::INACTIVE) {
        spdlog::warn("InputManager::onAction: ActionState::INACTIVE 不能用于绑定回调，将回退到 PRESSED。");
        action_state = ActionState::PRESSED;
    }

    const auto index = static_cast<std::size_t>(action_state);
    if (index >= ActionEntry::CALLBACK_STATE_COUNT) {
        spdlog::warn("InputManager::onAction: 非法 ActionState={}，将回退到 PRESSED。", index);
        return actions_[action_name_id].signals[0];
    }

    // 如果 action_name 不存在，自动创建一条 ActionEntry（懒加载）
    return actions_[action_name_id].signals[index];
}

// --- 更新和事件处理 ---

void InputManager::update() {
    // 兼容旧路径：每次 update 视作"一个逻辑 tick + 一次输入采样"。
    consumeTick();
    sampleInputEvents();
    dispatchActionCallbacks();
}

void InputManager::sampleInputEvents() {
    // 处理所有待处理的 SDL 事件（这将更新动作状态与鼠标数据）
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
            // RmlUi 6.x: true=继续传播，false=已消费（5.x 语义相反）。
            should_propagate = rmlui_event_callback_(event);
        }

        // 即使被 UI 消费，也要放行释放事件，避免动作状态卡在 HELD。
        const bool is_release_event =
            (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_MOUSE_BUTTON_UP);

        if (!should_propagate && !is_release_event) {
            continue;
        }

        processEvent(event);
    }

}

void InputManager::dispatchActionCallbacks() {
    // 按动作名称字母序遍历，保证同帧多动作的分发顺序跨平台稳定
    for (auto action_id : action_dispatch_order_) {
        auto& entry = actions_[action_id];
        if (entry.state != ActionState::INACTIVE) {
            entry.signals[static_cast<size_t>(entry.state)].collect([](bool result) {
                return result;
            });
        }
    }
}

void InputManager::consumeTick() {
    // 推进边沿状态生命周期
    for (auto& [_, entry] : actions_) {
        if (entry.state == ActionState::PRESSED) {
            entry.state = ActionState::HELD;
        } else if (entry.state == ActionState::RELEASED) {
            entry.state = ActionState::INACTIVE;
        }
    }

    // 滚轮数据视为"单 tick 有效"的一次性输入
    mouse_wheel_delta_ = {0.0f, 0.0f};
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
            if (block_keyboard && event.key.down) {
                break;
            }
            handleInputEdge(event.key.scancode, key_to_actions_, key_down_states_, actions_, event.key.down);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (block_mouse && event.button.down) {
                recalculateLogicalMousePosition();
                break;
            }
            handleInputEdge(static_cast<Uint32>(event.button.button), mouse_to_actions_, mouse_down_states_, actions_, event.button.down);
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
            mouse_wheel_delta_.x += event.wheel.x;
            mouse_wheel_delta_.y += event.wheel.y;
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
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MINIMIZED: {
            // 失焦时清空所有物理输入和动作状态，防止卡键。
            // 直接置 INACTIVE 而非 RELEASED：RELEASED 语义是"用户主动释放"，
            // 失焦是系统事件，不应触发 RELEASED 回调（如 UI onClick）。
            for (auto& [_, down] : key_down_states_) down = false;
            for (auto& [_, down] : mouse_down_states_) down = false;
            for (auto& [_, entry] : actions_) {
                entry.active_count = 0;
                entry.state = ActionState::INACTIVE;
            }
            dispatcher_->trigger<engine::utils::FocusLostEvent>();
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

    // SDL 的鼠标坐标（event.motion.x/y、SDL_GetMouseState）以"window coordinates"为单位，
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
    auto resolved_mappings = actions_to_keyname;
    key_to_actions_.clear();
    mouse_to_actions_.clear();
    key_down_states_.clear();
    mouse_down_states_.clear();
    actions_.clear();
    action_dispatch_order_.clear();

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
        auto action_name_id = entt::hashed_string(action_name.c_str());
        auto& entry = actions_[action_name_id];
        entry.name = action_name;
        action_dispatch_order_.push_back(action_name_id);
        spdlog::trace("映射动作: {}", action_name);
        // 设置 "按键 -> 动作" 的映射
        for (const auto& key_name : key_names) {
            SDL_Scancode scancode = scancodeFromString(key_name);       // 尝试根据按键名称获取scancode
            Uint32 mouse_button = mouseButtonFromString(key_name);  // 尝试根据按键名称获取鼠标按钮

            if (scancode != SDL_SCANCODE_UNKNOWN) {
                key_to_actions_[scancode].push_back(action_name_id);
                spdlog::trace("  映射按键: {} (Scancode: {}) 到动作: {}", key_name, static_cast<int>(scancode), action_name);
            } else if (mouse_button != 0) {
                mouse_to_actions_[mouse_button].push_back(action_name_id);
                spdlog::trace("  映射鼠标按钮: {} (Button ID: {}) 到动作: {}", key_name, static_cast<int>(mouse_button), action_name);
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
    // SDL_GetScancodeFromName 要求 null-terminated C 字符串；
    // string_view::data() 不保证 null-terminated，构造临时 std::string 确保安全。
    return SDL_GetScancodeFromName(std::string(key_name).c_str());
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
