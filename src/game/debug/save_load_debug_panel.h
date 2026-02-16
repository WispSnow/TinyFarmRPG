#pragma once

#include "engine/debug/debug_panel.h"

#include <string>

namespace game::save {
class SaveService;
}

namespace game::debug {

class SaveLoadDebugPanel final : public engine::debug::DebugPanel {
    game::save::SaveService& save_service_;
    std::string path_;
    int slot_{0};
    std::string status_;

public:
    explicit SaveLoadDebugPanel(game::save::SaveService& save_service);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug

