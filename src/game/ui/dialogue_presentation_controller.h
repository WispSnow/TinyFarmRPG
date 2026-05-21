#pragma once

#include "game/defs/events.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <glm/vec2.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::data {
class RpgCatalog;
}

namespace game::ui {

class DialogueBoxView;
class FloatingNoticeView;

class DialoguePresentationController final {
public:
    DialoguePresentationController(entt::dispatcher& dispatcher,
                                   entt::registry& registry,
                                   DialogueBoxView* conversation_box,
                                   FloatingNoticeView* notice_view,
                                   FloatingNoticeView* item_notice_view,
                                   const game::data::RpgCatalog* rpg_catalog,
                                   glm::vec2 notice_offset = {0.0F, -4.0F},
                                   glm::vec2 item_notice_offset = {0.0F, -56.0F});
    ~DialoguePresentationController();

private:
    struct NoticeSlot {
        FloatingNoticeView* view{nullptr};
        glm::vec2 screen_offset{0.0F, -4.0F};
    };

    entt::dispatcher& dispatcher_;
    entt::registry& registry_;
    DialogueBoxView* conversation_box_{nullptr};
    NoticeSlot notice_slot_{};
    NoticeSlot item_notice_slot_{};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    std::unordered_map<entt::id_type, std::string> portrait_by_actor_id_hash_{};
    std::unordered_map<entt::id_type, std::string> portrait_by_map_actor_id_hash_{};

    void onShow(const game::defs::DialogueShowEvent& evt);
    void onMove(const game::defs::DialogueMoveEvent& evt);
    void onHide(const game::defs::DialogueHideEvent& evt);

    void buildPortraitCache();
    [[nodiscard]] std::string resolvePortraitDecorator(const game::defs::DialogueShowEvent& evt) const;
    [[nodiscard]] const NoticeSlot* noticeSlot(game::defs::DialogueChannel channel) const;
    [[nodiscard]] NoticeSlot* noticeSlot(game::defs::DialogueChannel channel);
    [[nodiscard]] static std::string formatNoticeText(std::string_view speaker, std::string_view text);
};

} // namespace game::ui
