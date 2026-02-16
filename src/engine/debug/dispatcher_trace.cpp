#include "dispatcher_trace.h"

#include <algorithm>
#include <cassert>

namespace engine::debug {

DispatcherTrace::ScopedQueueDispatch::ScopedQueueDispatch(DispatcherTrace& trace)
    : trace_(trace) {
    trace_.beginQueueDispatch();
}

DispatcherTrace::ScopedQueueDispatch::~ScopedQueueDispatch() {
    trace_.endQueueDispatch();
}

DispatcherTrace::DispatcherTrace(std::size_t max_dispatches) {
    max_dispatches_ = std::max<std::size_t>(max_dispatches, 1U);
    dispatches_.resize(max_dispatches_);
}

DispatcherTrace::~DispatcherTrace() {
    if (dispatcher_) {
        dispatcher_->disconnect(this);
    }
}

std::vector<DispatcherTrace::RecentDispatch> DispatcherTrace::recentDispatches(std::size_t max_count) const {
    if (dispatch_count_ == 0U || max_count == 0U) {
        return {};
    }

    const std::size_t count = std::min(max_count, dispatch_count_);
    std::vector<RecentDispatch> out;
    out.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t index = (dispatch_cursor_ + max_dispatches_ - 1U - i) % max_dispatches_;
        const auto& entry = dispatches_[index];
        RecentDispatch snapshot;
        snapshot.sequence = entry.sequence;
        snapshot.queued = entry.timing == DispatchTiming::Queued;
        snapshot.event_name = entry.event_index < event_names_.size() ? std::string_view{event_names_[entry.event_index]} : std::string_view{};
        out.push_back(snapshot);
    }

    return out;
}

void DispatcherTrace::bindDispatcher(entt::dispatcher& dispatcher) {
    if (dispatcher_ == nullptr) {
        dispatcher_ = &dispatcher;
        return;
    }
    assert(dispatcher_ == &dispatcher && "DispatcherTrace can only watch a single dispatcher instance");
}

std::size_t DispatcherTrace::registerEvent(const std::type_index& key, std::string_view name) {
    const std::size_t index = event_names_.size();
    event_names_.emplace_back(name);
    event_index_by_type_.emplace(key, index);
    return index;
}

void DispatcherTrace::record(const std::type_index& key) {
    const auto it = event_index_by_type_.find(key);
    if (it == event_index_by_type_.end()) {
        return;
    }

    const std::size_t index = it->second;
    const auto timing = isQueueDispatchActive() ? DispatchTiming::Queued : DispatchTiming::Immediate;

    recordDispatch(index, timing);
}

void DispatcherTrace::recordDispatch(std::size_t event_index, DispatchTiming timing) {
    if (max_dispatches_ == 0U || dispatches_.empty()) {
        return;
    }

    DispatchEntry entry;
    entry.sequence = ++dispatch_sequence_;
    entry.event_index = event_index;
    entry.timing = timing;

    dispatches_[dispatch_cursor_] = entry;
    dispatch_cursor_ = (dispatch_cursor_ + 1U) % max_dispatches_;
    dispatch_count_ = std::min(dispatch_count_ + 1U, max_dispatches_);
}

void DispatcherTrace::beginQueueDispatch() {
    ++queue_dispatch_depth_;
}

void DispatcherTrace::endQueueDispatch() {
    if (queue_dispatch_depth_ == 0) {
        return;
    }
    --queue_dispatch_depth_;
}

} // namespace engine::debug
