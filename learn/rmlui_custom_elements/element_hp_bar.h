#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementInstancer.h>

#include <algorithm>
#include <string>

namespace learn::rmlui {

/// 自定义 <hp-bar> 元素：接受 value / max / bar-color 属性，内部渲染填充条
///
/// 用法示例：
///   <hp-bar value="80" max="100"/>                     <!-- HP 自动变色 -->
///   <hp-bar value="45" max="60" bar-color="#1f6feb"/>  <!-- 固定蓝色 MP -->
///
/// 配合数据绑定：
///   <hp-bar data-attr-value="hp" data-attr-max="max_hp"/>
///
class ElementHpBar : public Rml::Element {
public:
    explicit ElementHpBar(const Rml::String& tag) : Rml::Element(tag) {}

    /// 属性变化时更新填充条
    void OnAttributeChange(const Rml::ElementAttributes& changed) override {
        Element::OnAttributeChange(changed);

        if (auto it = changed.find("value"); it != changed.end())
            value_ = it->second.Get<float>(0.0f);
        if (auto it = changed.find("max"); it != changed.end())
            max_ = it->second.Get<float>(100.0f);
        if (auto it = changed.find("bar-color"); it != changed.end())
            fixed_color_ = it->second.Get<Rml::String>("");

        updateFill();
    }

    /// 元素被加入 DOM 时创建内部填充子元素
    void OnChildAdd(Rml::Element* child) override {
        Element::OnChildAdd(child);
        if (child == this) {
            // 读取初始属性（可能在 OnChildAdd 前已设置）
            value_ = GetAttribute("value", 0.0f);
            max_   = GetAttribute("max", 100.0f);
            fixed_color_ = GetAttribute("bar-color", Rml::String(""));
            createFill();
        }
    }

private:
    void createFill() {
        if (fill_) return;
        auto* doc = GetOwnerDocument();
        if (!doc) return;

        auto el = doc->CreateElement("div");
        el->SetClassNames("hp-bar-fill");
        fill_ = AppendChild(std::move(el));
        updateFill();
    }

    void updateFill() {
        if (!fill_) return;

        float pct = (max_ > 0.0f) ? (value_ / max_ * 100.0f) : 0.0f;
        pct = std::clamp(pct, 0.0f, 100.0f);

        fill_->SetProperty("width", std::to_string(static_cast<int>(pct)) + "%");

        if (!fixed_color_.empty()) {
            // 固定颜色模式（如 MP 条）
            fill_->SetProperty("background-color", fixed_color_);
        } else {
            // HP 自动变色：绿 → 黄 → 红
            if (pct >= 70.0f)
                fill_->SetProperty("background-color", "#2ea043");
            else if (pct >= 30.0f)
                fill_->SetProperty("background-color", "#e0af68");
            else
                fill_->SetProperty("background-color", "#f7768e");
        }
    }

    float value_ = 0.0f;
    float max_   = 100.0f;
    Rml::String fixed_color_;
    Rml::Element* fill_ = nullptr;
};

} // namespace learn::rmlui
