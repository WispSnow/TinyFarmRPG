#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <entt/signal/dispatcher.hpp>

namespace engine::debug {

class DebugUIManager;

/**
 * @brief Lightweight event dispatch tracer for entt::dispatcher.
 *
 * Records the last N dispatches of watched events, separated into:
 * - immediate dispatches (typically via dispatcher.trigger)
 * - queued dispatches (dispatched during dispatcher.update)
 *
 * Note: This tracer classifies by *dispatch timing*, not by the call site.
 * If a listener triggers events while queue dispatch is active, those will be
 * counted as queued.
 */
class DispatcherTrace final {
    enum class DispatchTiming : std::uint8_t {
        Immediate = 0,
        Queued = 1
    };

    struct DispatchEntry {
        std::uint64_t sequence{0};
        std::size_t event_index{0};
        DispatchTiming timing{DispatchTiming::Immediate};
    };

    entt::dispatcher* dispatcher_{nullptr};

    std::vector<std::string> event_names_;
    std::unordered_map<std::type_index, std::size_t> event_index_by_type_;

    std::size_t max_dispatches_{0};
    std::size_t dispatch_cursor_{0};
    std::size_t dispatch_count_{0};
    std::uint64_t dispatch_sequence_{0};
    std::vector<DispatchEntry> dispatches_;

    std::uint32_t queue_dispatch_depth_{0};

public:
    struct RecentDispatch final {
        std::uint64_t sequence{0};
        std::string_view event_name{};
        bool queued{false};
    };

    class ScopedQueueDispatch final {
        DispatcherTrace& trace_;

    public:
        explicit ScopedQueueDispatch(DispatcherTrace& trace);
        ~ScopedQueueDispatch();

        ScopedQueueDispatch(const ScopedQueueDispatch&) = delete;
        ScopedQueueDispatch& operator=(const ScopedQueueDispatch&) = delete;
        ScopedQueueDispatch(ScopedQueueDispatch&&) = delete;
        ScopedQueueDispatch& operator=(ScopedQueueDispatch&&) = delete;
    };

    explicit DispatcherTrace(std::size_t max_dispatches = 512);
    ~DispatcherTrace();

    DispatcherTrace(const DispatcherTrace&) = delete;
    DispatcherTrace& operator=(const DispatcherTrace&) = delete;
    DispatcherTrace(DispatcherTrace&&) = delete;
    DispatcherTrace& operator=(DispatcherTrace&&) = delete;

    [[nodiscard]] std::vector<RecentDispatch> recentDispatches(std::size_t max_count) const;

    template<typename Event>
    void watch(entt::dispatcher& dispatcher, std::string_view name) {
        const std::type_index key{typeid(Event)};
        if (event_index_by_type_.contains(key)) {
            return;
        }

        const std::size_t index = registerEvent(key, name);
        bindDispatcher(dispatcher);
        dispatcher.sink<Event>().template connect<&DispatcherTrace::onEvent<Event>>(this);
        (void)index;
    }

private:
    void bindDispatcher(entt::dispatcher& dispatcher);
    [[nodiscard]] std::size_t registerEvent(const std::type_index& key, std::string_view name);

    template<typename Event>
    void onEvent(const Event&) {
        record(std::type_index{typeid(Event)});
    }

    void record(const std::type_index& key);
    void recordDispatch(std::size_t event_index, DispatchTiming timing);
    void beginQueueDispatch();
    void endQueueDispatch();

    [[nodiscard]] bool isQueueDispatchActive() const { return queue_dispatch_depth_ > 0; }

    friend class ScopedQueueDispatch;
    friend class DebugUIManager;
};

} // namespace engine::debug
