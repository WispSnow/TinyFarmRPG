#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <type_traits>

#include "debug_panel.h"
#include "dispatcher_trace.h"

namespace engine::debug {

/**
 * @brief 调试面板分类枚举，用于区分不同类别的调试面板。
 */
enum class PanelCategory : size_t {
    Engine = 0,  ///< @brief 引擎层面板（默认快捷键：F5）
    Game = 1,    ///< @brief 游戏层面板（默认快捷键：F6）
    Count        ///< @brief 面板类别总数（用于数组大小）
};

/**
 * @brief 管理所有调试面板，可集中控制可见性、注册与注销。
 */
class DebugUIManager final {
    struct PanelEntry {
        std::unique_ptr<DebugPanel> panel;
        std::string name;
        bool enabled{false};
    };

    static constexpr size_t PANEL_CATEGORY_COUNT = static_cast<size_t>(PanelCategory::Count);
    
    std::unique_ptr<DispatcherTrace> dispatcher_trace_;

    std::array<bool, PANEL_CATEGORY_COUNT> visible_{};
    std::array<std::vector<PanelEntry>, PANEL_CATEGORY_COUNT> panels_{};

    static constexpr const char* getCategoryName(PanelCategory category) {
        switch (category) {
            case PanelCategory::Engine:
                return "Engine Debug Panels";
            case PanelCategory::Game:
                return "Game Debug Panels";
            default:
                return "Debug Panels";
        }
    }

public:
    DebugUIManager() = default;
    ~DebugUIManager();

    DebugUIManager(const DebugUIManager&) = delete;
    DebugUIManager& operator=(const DebugUIManager&) = delete;
    DebugUIManager(DebugUIManager&&) = delete;
    DebugUIManager& operator=(DebugUIManager&&) = delete;

    void toggleVisible(PanelCategory category = PanelCategory::Engine);
    void setVisible(bool visible, PanelCategory category = PanelCategory::Engine);
    [[nodiscard]] bool isVisible(PanelCategory category = PanelCategory::Engine) const {
        return visible_[static_cast<size_t>(category)];
    }

    void enableDispatcherTrace(entt::dispatcher& dispatcher);

    void onDispatcherUpdateBegin();
    void onDispatcherUpdateEnd();

    void draw();        ///< @brief 绘制控制面板及所有启用的调试面板。

    /**
     * @brief 注册调试面板。
     * @param panel 面板实例所有权。
     * @param enabled 是否默认启用。
     * @param category 面板分类（Engine=引擎层，Game=游戏层）。
     * @return 面板的裸指针，便于调用方缓存引用。
     */
    DebugPanel* registerPanel(std::unique_ptr<DebugPanel> panel, 
                              bool enabled = false, 
                              PanelCategory category = PanelCategory::Engine);

    /**
     * @brief 注销调试面板。
     * @param panel 需要注销的面板指针。
     * @param category 面板分类（Engine=引擎层，Game=游戏层）。
     */
    void unregisterPanel(const DebugPanel* panel, PanelCategory category = PanelCategory::Engine);
    /**
     * @brief 按类别注销所有调试面板（会触发已启用面板的 onHide）。
     * @param category 面板分类（Engine=引擎层，Game=游戏层）。
     */
    void unregisterPanels(PanelCategory category = PanelCategory::Engine);

    /**
     * @brief 获取面板类别总数。
     * @return 面板类别数量。
     */
    [[nodiscard]] static constexpr size_t getCategoryCount() {
        return PANEL_CATEGORY_COUNT;
    }

    template<typename T>
    [[nodiscard]] T* getPanel(PanelCategory category = PanelCategory::Engine) const {
        static_assert(std::is_base_of_v<DebugPanel, T>, "T must derive from DebugPanel");
        const size_t index = static_cast<size_t>(category);
        if (index >= panels_.size()) {
            return nullptr;
        }
        const auto& panel_list = panels_[index];
        for (const auto& entry : panel_list) {
            if (auto* typed = dynamic_cast<T*>(entry.panel.get())) {
                return typed;
            }
        }
        return nullptr;
    }

private:
    void drawHubWindow(PanelCategory category);   ///< @brief 所有窗口的开关集合
};

} // namespace engine::debug
