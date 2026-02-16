#pragma once
#include "interaction_behavior.h"
#include <functional>

namespace engine::ui {

class DragBehavior final : public InteractionBehavior {
public:
    using DragBeginCallback = std::function<void(UIInteractive&, const glm::vec2&)>;
    using DragUpdateCallback = std::function<void(UIInteractive&, const glm::vec2&, const glm::vec2&)>;
    using DragEndCallback = std::function<void(UIInteractive&, const glm::vec2&, bool)>;

private:
    DragBeginCallback on_begin_{};
    DragUpdateCallback on_update_{};
    DragEndCallback on_end_{};

public:
    DragBehavior() = default;
    ~DragBehavior() override = default;

    void setOnBegin(DragBeginCallback cb) { on_begin_ = std::move(cb); }
    void setOnUpdate(DragUpdateCallback cb) { on_update_ = std::move(cb); }
    void setOnEnd(DragEndCallback cb) { on_end_ = std::move(cb); }

    void onDragBegin(UIInteractive& owner, const glm::vec2& pos) override {
        if (on_begin_) on_begin_(owner, pos);
    }

    void onDragUpdate(UIInteractive& owner, const glm::vec2& pos, const glm::vec2& delta) override {
        if (on_update_) on_update_(owner, pos, delta);
    }

    void onDragEnd(UIInteractive& owner, const glm::vec2& pos, bool accepted) override {
        if (on_end_) on_end_(owner, pos, accepted);
    }
};

} // namespace engine::ui
