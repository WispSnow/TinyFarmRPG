#include "dialogue_presentation_controller.h"

#include "dialogue_box_view.h"
#include "floating_notice_view.h"
#include "engine/component/name_component.h"
#include "game/component/recruitable_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <spdlog/fmt/fmt.h>

#include <utility>

namespace {

constexpr std::string_view NONE_DECORATOR = "none";

[[nodiscard]] std::string portraitDecoratorForActor(const game::data::ActorData& actor) {
    if (actor.portrait_.decorator_.empty()) {
        return std::string{NONE_DECORATOR};
    }
    return fmt::format("image({})", actor.portrait_.decorator_);
}

} // namespace

namespace game::ui {

DialoguePresentationController::DialoguePresentationController(entt::dispatcher& dispatcher,
                                                               entt::registry& registry,
                                                               DialogueBoxView* conversation_box,
                                                               FloatingNoticeView* notice_view,
                                                               FloatingNoticeView* item_notice_view,
                                                               const game::data::RpgCatalog* rpg_catalog,
                                                               glm::vec2 notice_offset,
                                                               glm::vec2 item_notice_offset)
    : dispatcher_(dispatcher),
      registry_(registry),
      conversation_box_(conversation_box),
      notice_slot_{.view = notice_view, .screen_offset = notice_offset},
      item_notice_slot_{.view = item_notice_view, .screen_offset = item_notice_offset},
      rpg_catalog_(rpg_catalog) {
    buildPortraitCache();
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialoguePresentationController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().connect<&DialoguePresentationController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialoguePresentationController::onHide>(this);
}

DialoguePresentationController::~DialoguePresentationController() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().disconnect<&DialoguePresentationController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().disconnect<&DialoguePresentationController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&DialoguePresentationController::onHide>(this);
}

void DialoguePresentationController::buildPortraitCache() {
    portrait_by_actor_id_hash_.clear();
    portrait_by_map_actor_id_hash_.clear();
    if (!rpg_catalog_) {
        return;
    }

    for (const auto* actor : rpg_catalog_->listActors()) {
        if (!actor) {
            continue;
        }
        const std::string decorator = portraitDecoratorForActor(*actor);
        if (decorator == NONE_DECORATOR) {
            continue;
        }
        portrait_by_actor_id_hash_.insert_or_assign(actor->id_hash_, decorator);
        if (actor->map_actor_id_hash_ != entt::null) {
            portrait_by_map_actor_id_hash_.insert_or_assign(actor->map_actor_id_hash_, decorator);
        }
    }
}

void DialoguePresentationController::onShow(const game::defs::DialogueShowEvent& evt) {
    if (evt.channel == game::defs::DialogueChannel::Conversation) {
        if (!conversation_box_) {
            return;
        }
        conversation_box_->setSpeaker(evt.speaker);
        conversation_box_->setText(evt.text);
        conversation_box_->setPortraitDecorator(resolvePortraitDecorator(evt));
        conversation_box_->setVisible(true);
        return;
    }

    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->view->setWorldAnchor(evt.world_position, slot->screen_offset);
    slot->view->setText(formatNoticeText(evt.speaker, evt.text));
    slot->view->setVisible(true);
}

void DialoguePresentationController::onMove(const game::defs::DialogueMoveEvent& evt) {
    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->view->setWorldAnchor(evt.world_position, slot->screen_offset);
}

void DialoguePresentationController::onHide(const game::defs::DialogueHideEvent& evt) {
    if (evt.channel == game::defs::DialogueChannel::Conversation) {
        if (conversation_box_) {
            conversation_box_->setVisible(false);
        }
        return;
    }

    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->view->clearWorldAnchor();
    slot->view->setVisible(false);
}

std::string DialoguePresentationController::resolvePortraitDecorator(const game::defs::DialogueShowEvent& evt) const {
    if (evt.speaker_actor_id_hash != entt::null) {
        if (const auto it = portrait_by_actor_id_hash_.find(evt.speaker_actor_id_hash);
            it != portrait_by_actor_id_hash_.end()) {
            return it->second;
        }
    }

    if (const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(evt.target)) {
        if (const auto it = portrait_by_actor_id_hash_.find(recruitable->actor_id_hash_);
            it != portrait_by_actor_id_hash_.end()) {
            return it->second;
        }
    }

    if (const auto* name = registry_.try_get<engine::component::NameComponent>(evt.target)) {
        if (const auto it = portrait_by_map_actor_id_hash_.find(name->name_id_);
            it != portrait_by_map_actor_id_hash_.end()) {
            return it->second;
        }
    }

    return std::string{NONE_DECORATOR};
}

const DialoguePresentationController::NoticeSlot* DialoguePresentationController::noticeSlot(
    game::defs::DialogueChannel channel) const {
    switch (channel) {
    case game::defs::DialogueChannel::Notice:
        return &notice_slot_;
    case game::defs::DialogueChannel::ItemNotice:
        return &item_notice_slot_;
    case game::defs::DialogueChannel::Conversation:
        return nullptr;
    }
    return nullptr;
}

DialoguePresentationController::NoticeSlot* DialoguePresentationController::noticeSlot(
    game::defs::DialogueChannel channel) {
    return const_cast<NoticeSlot*>(std::as_const(*this).noticeSlot(channel));
}

std::string DialoguePresentationController::formatNoticeText(std::string_view speaker, std::string_view text) {
    std::string output;
    output.reserve(speaker.size() + text.size() + 4);
    if (!speaker.empty()) {
        output.append(speaker);
        output.append(":\n");
    }
    output.append(text);
    return output;
}

} // namespace game::ui
