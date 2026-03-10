#pragma once

#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
}

class SystemInterface_SDL;

namespace engine::ui::rmlui {

class RenderInterface_GL3_STB;

class RmlUILayer final {
public:
    [[nodiscard]] static std::unique_ptr<RmlUILayer> create(SDL_Window* window,
                                                             int viewport_width,
                                                             int viewport_height,
                                                             int viewport_offset_x = 0,
                                                             int viewport_offset_y = 0);
    ~RmlUILayer();

    RmlUILayer(const RmlUILayer&) = delete;
    RmlUILayer& operator=(const RmlUILayer&) = delete;
    RmlUILayer(RmlUILayer&&) = delete;
    RmlUILayer& operator=(RmlUILayer&&) = delete;

    void clean();

    [[nodiscard]] bool processEvent(SDL_Event& event);
    void update();
    void render();
    void setViewport(int width, int height, int offset_x = 0, int offset_y = 0);

    /// 设置逻辑分辨率。Context 以此为布局空间，dp_ratio 自动计算为 viewport/logical。
    /// 未设置时 Context 直接使用物理 viewport 尺寸。
    void setLogicalSize(int width, int height);

    void setTextureFilterMode(RmlUiTextureFilterMode mode);
    [[nodiscard]] RmlUiTextureFilterMode getTextureFilterMode() const;

    // --- 多文档管理 ---

    /// 加载文档并关联到 owner 场景实例。返回文档指针，失败返回 nullptr。
    /// @param owner_scene_id  场景实例 ID（0 表示全局文档，任何场景下均可交互）。
    [[nodiscard]] Rml::ElementDocument* loadDocument(std::string_view document_path,
                                                      uint64_t owner_scene_id = 0);

    /// 卸载单个文档。
    void unloadDocument(Rml::ElementDocument* doc);

    /// 卸载指定 owner 实例 ID 名下的所有文档。
    void unloadDocumentsByOwner(uint64_t owner_scene_id);

    /// 显示/隐藏文档。
    void showDocument(Rml::ElementDocument* doc);
    void hideDocument(Rml::ElementDocument* doc);

    // --- 场景归属 ---

    /// 设置活跃场景实例 ID。非活跃场景的文档将禁止交互并失去焦点。
    void setActiveScene(uint64_t scene_id);

    [[nodiscard]] uint64_t getActiveSceneId() const { return active_scene_id_; }

    // --- 向后兼容 (过渡期) ---

    /// 重新加载最近加载的文档（调试用）。
    [[nodiscard]] bool reloadLastDocument();

    [[nodiscard]] Rml::Context* getContext() const { return context_; }

    // --- 调试信息 ---

    [[nodiscard]] size_t getDocumentCount() const { return documents_.size(); }

    /// 遍历所有已加载文档，回调签名: void(const std::string& path, uint64_t owner)
    template<typename Fn>
    void forEachDocument(Fn&& fn) const {
        for (const auto& entry : documents_) {
            fn(entry.path, entry.owner);
        }
    }

private:
    RmlUILayer() = default;

    [[nodiscard]] bool init(SDL_Window* window,
                            int viewport_width,
                            int viewport_height,
                            int viewport_offset_x,
                            int viewport_offset_y);

    void adjustEventForViewport(SDL_Event& event) const;
    void applyInteractionPolicy();

    struct DocumentEntry {
        Rml::ElementDocument* doc{nullptr};
        uint64_t owner{0};   ///< 场景实例 ID，0 = 全局（无归属）
        std::string path;
    };

    SDL_Window* window_{nullptr};
    std::unique_ptr<RenderInterface_GL3_STB> render_interface_;
    std::unique_ptr<SystemInterface_SDL> system_interface_;
    Rml::Context* context_{nullptr};

    std::vector<DocumentEntry> documents_;
    uint64_t active_scene_id_{0};

    int viewport_width_{1};
    int viewport_height_{1};
    int viewport_offset_x_{0};
    int viewport_offset_y_{0};

    int logical_width_{0};   ///< 逻辑分辨率宽（0 = 未设置，使用 viewport）
    int logical_height_{0};  ///< 逻辑分辨率高

    bool initialized_{false};
};

} // namespace engine::ui::rmlui
