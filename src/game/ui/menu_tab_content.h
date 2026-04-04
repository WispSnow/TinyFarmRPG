#pragma once

namespace Rml {
class DataModelConstructor;
}

namespace game::ui {

enum class MenuTabId {
    Inventory = 0,
    Equipment = 1,
    Quests = 2,
    Map = 3,
    Options = 4,
};

/// 菜单标签页内容的统一接口。
/// Scene 负责文档与生命周期，Tab 内容只关注自身绑定和交互行为。
class IMenuTabContent {
public:
    virtual ~IMenuTabContent() = default;

    [[nodiscard]] virtual bool bindModel(Rml::DataModelConstructor& constructor) = 0;
    virtual void onActivated() = 0;
    virtual void onDeactivated() = 0;
    virtual void update(float delta_time) = 0;
    [[nodiscard]] virtual bool onCancel() = 0;
};

} // namespace game::ui
