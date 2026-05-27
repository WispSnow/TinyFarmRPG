#include "game/ui/player_portrait_service.h"

#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"
#include "game/component/appearance_component.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <utility>

namespace game::ui {

namespace {

[[nodiscard]] std::string kindSuffix(const PortraitImageKind kind) {
    switch (kind) {
        case PortraitImageKind::Standard64:
            return "standard64";
        case PortraitImageKind::Standard64Linear:
            return "standard64-linear";
        case PortraitImageKind::Battle48:
            return "battle48";
    }
    return "standard64";
}

[[nodiscard]] engine::ui::rmlui::RmlUiTextureFilterMode filterModeForKind(const PortraitImageKind kind) {
    switch (kind) {
        case PortraitImageKind::Standard64Linear:
            return engine::ui::rmlui::RmlUiTextureFilterMode::Linear;
        case PortraitImageKind::Standard64:
        case PortraitImageKind::Battle48:
            return engine::ui::rmlui::RmlUiTextureFilterMode::Nearest;
    }
    return engine::ui::rmlui::RmlUiTextureFilterMode::Nearest;
}

[[nodiscard]] engine::resource::DecodedImage imageForKind(const AppearancePortraitImages& images,
                                                          const PortraitImageKind kind) {
    switch (kind) {
        case PortraitImageKind::Standard64:
        case PortraitImageKind::Standard64Linear:
            return images.standard64;
        case PortraitImageKind::Battle48:
            return images.battle48;
    }
    return images.standard64;
}

} // namespace

PlayerPortraitService::PlayerPortraitService(
    entt::dispatcher& dispatcher,
    entt::registry& registry,
    engine::ui::rmlui::RmlGeneratedImageRegistry& generated_images,
    const game::data::AppearanceCatalog& catalog,
    const entt::entity player,
    std::string source_prefix)
    : dispatcher_(dispatcher),
      registry_(registry),
      generated_images_(generated_images),
      catalog_(catalog),
      player_(player),
      source_prefix_(std::move(source_prefix)) {
    dispatcher_.sink<game::defs::AppearanceChangedEvent>().connect<&PlayerPortraitService::onAppearanceChanged>(this);
    (void)refresh();
}

PlayerPortraitService::~PlayerPortraitService() {
    dispatcher_.sink<game::defs::AppearanceChangedEvent>().disconnect<&PlayerPortraitService::onAppearanceChanged>(this);
}

bool PlayerPortraitService::refresh() {
    if (player_ == entt::null || !registry_.valid(player_)) {
        ready_ = false;
        source_uris_ = {};
        for (auto& registration : registrations_) {
            registration.reset();
        }
        return false;
    }
    const auto* appearance = registry_.try_get<game::component::AppearanceComponent>(player_);
    if (!appearance) {
        ready_ = false;
        source_uris_ = {};
        for (auto& registration : registrations_) {
            registration.reset();
        }
        return false;
    }

    auto images = builder_.build(catalog_, *appearance);
    if (!images.valid()) {
        spdlog::warn("PlayerPortraitService: failed to build dynamic player portrait.");
        return false;
    }

    const std::array<PortraitImageKind, 3> kinds{
        PortraitImageKind::Standard64,
        PortraitImageKind::Standard64Linear,
        PortraitImageKind::Battle48,
    };
    std::array<std::string, 3> next_source_uris{};
    for (const auto kind : kinds) {
        const std::size_t index = kindIndex(kind);
        next_source_uris[index] = source_prefix_ + "/" + images.selection_key + "/" + kindSuffix(kind);
    }

    if (next_source_uris == source_uris_) {
        bool registrations_valid = true;
        for (const auto& registration : registrations_) {
            registrations_valid = registrations_valid && registration.valid();
        }
        if (ready_ && registrations_valid) {
            return true;
        }

        ready_ = false;
        source_uris_ = {};
        for (auto& registration : registrations_) {
            registration.reset();
        }
    }

    std::array<engine::ui::rmlui::RmlGeneratedImageRegistry::Registration, 3> next_registrations{};
    for (const auto kind : kinds) {
        const std::size_t index = kindIndex(kind);
        next_registrations[index] = generated_images_.registerImage(
            next_source_uris[index],
            imageForKind(images, kind),
            filterModeForKind(kind));
        if (!next_registrations[index].valid()) {
            return false;
        }
    }

    registrations_ = std::move(next_registrations);
    source_uris_ = std::move(next_source_uris);
    ready_ = true;
    return true;
}

std::string_view PlayerPortraitService::sourceUri(const PortraitImageKind kind) const {
    if (!ready_) {
        return {};
    }
    return source_uris_[kindIndex(kind)];
}

std::string PlayerPortraitService::decoratorString(const PortraitImageKind kind) const {
    const std::string_view source = sourceUri(kind);
    if (source.empty()) {
        return "none";
    }
    return "image(" + std::string(source) + ")";
}

void PlayerPortraitService::onAppearanceChanged(const game::defs::AppearanceChangedEvent& event) {
    if (event.target != player_) {
        return;
    }
    (void)refresh();
}

std::size_t PlayerPortraitService::kindIndex(const PortraitImageKind kind) noexcept {
    switch (kind) {
        case PortraitImageKind::Standard64:
            return 0U;
        case PortraitImageKind::Standard64Linear:
            return 1U;
        case PortraitImageKind::Battle48:
            return 2U;
    }
    return 0U;
}

} // namespace game::ui
