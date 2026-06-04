#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace Rml {
class Event;
class DataModelHandle;
class DataModelConstructor;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::scene {

/**
 * @brief 存档槽选择场景，支持存档（Save）和读档（Load）两种模式。
 *
 * 工作流程：
 * - 扫描所有存档槽并构建 SlotViewModel 列表，通过 RmlUi data model 驱动 UI。
 * - Load 模式：仅已有存档的槽可选；点击后直接调用 on_select_ 回调。
 * - Save 模式：空槽和已有存档槽均可选；点击已有存档槽时弹出覆盖确认对话框，
 *   确认后再调用 on_select_ 回调。
 */
class SaveSlotSelectScene final : public engine::scene::Scene {
public:
    /// 槽选中后的回调，参数为槽编号（0-based）。
    using SlotSelectCallback = std::function<void(int slot)>;

    enum class Mode {
        Load, ///< 读档模式：仅已有存档可选。
        Save, ///< 存档模式：空槽也可选；已有存档槽点击时弹出覆盖确认。
        Delete, ///< 删除模式：仅已有或损坏存档可选；点击后弹出删除确认。
    };

private:
    /// RmlUi data model 中每个槽的视图数据。
    struct SlotViewModel {
        int slot_index{0};      ///< 槽编号（0-based），用于事件回调中识别目标槽。
        Rml::String label{};    ///< 显示文本，例如 "Empty" / "Day 3 - 26-04-02 15:30" / "Invalid"。
        bool enabled{true};     ///< 是否可点击（Load 模式下空槽禁用；读取出错的槽禁用）。
    };

    SlotSelectCallback on_select_{};              ///< 选中槽后的回调。
    Mode mode_{Mode::Load};
    const game::runtime::LocalizationService* localization_{nullptr};
    std::optional<int> pending_overwrite_slot_{}; ///< 等待覆盖确认的槽编号；无值表示确认框未激活。
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    std::vector<SlotViewModel> slots_{};          ///< 与 RmlUi data model 绑定的槽列表。
    bool confirm_visible_{false};                 ///< 控制覆盖确认对话框的显示/隐藏。
    Rml::String confirm_text_{"!save_slot.confirm.overwrite!"}; ///< 确认对话框文本，含具体槽编号。
    bool data_types_registered_{false};

public:
    SaveSlotSelectScene(std::string_view name,
                        engine::core::Context& context,
                        SlotSelectCallback on_select = {},
                        Mode mode = Mode::Load,
                        const game::runtime::LocalizationService* localization = nullptr);
    ~SaveSlotSelectScene() override;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    /// 向 RmlUi 注册 SlotViewModel 结构体及 slots_ 容器类型（每个 DataTypeRegister 只注册一次）。
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void disconnectRuntimeListeners();
    /// 重新扫描磁盘并刷新 slots_ 列表及 UI。
    void refreshSlotButtons();
    void refreshLocalizedBindings();
    void refreshOverwriteConfirmText();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);

    void onSlotClicked(int slot);
    void onBackClicked();
    bool onMenuCancelPressed();

    void showOverwriteConfirm(int slot);
    void hideOverwriteConfirm();
    void onOverwriteConfirmYes();
    void onOverwriteConfirmNo();

    /// RmlUi data event 回调，从 arguments[0] 中解析槽编号后转发给 onSlotClicked。
    void onSlotSelectEvent(Rml::DataModelHandle model, Rml::Event& event, const Rml::VariantList& arguments);
};

} // namespace game::scene
