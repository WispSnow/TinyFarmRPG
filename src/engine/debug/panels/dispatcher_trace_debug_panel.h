#pragma once

#include "engine/debug/debug_panel.h"

namespace engine::debug {

class DispatcherTrace;

class DispatcherTraceDebugPanel final : public DebugPanel {
    DispatcherTrace& trace_;
    int recent_dispatches_{10};

public:
    explicit DispatcherTraceDebugPanel(DispatcherTrace& trace);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace engine::debug
