#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace game::script {

using ScriptStateValue = std::variant<std::nullptr_t, bool, double, std::string>;
using ScriptStateValueMap = std::unordered_map<std::string, ScriptStateValue>;

class ScriptStateStore final {
public:
    [[nodiscard]] const ScriptStateValueMap& values() const noexcept {
        return values_;
    }

    void replaceAll(ScriptStateValueMap values) {
        values_ = std::move(values);
    }

    void clear() noexcept {
        values_.clear();
    }

    [[nodiscard]] bool has(std::string_view key) const {
        return values_.contains(std::string{key});
    }

    [[nodiscard]] const ScriptStateValue* find(std::string_view key) const {
        const auto it = values_.find(std::string{key});
        return it == values_.end() ? nullptr : &it->second;
    }

    void set(std::string key, ScriptStateValue value) {
        values_.insert_or_assign(std::move(key), std::move(value));
    }

    [[nodiscard]] bool erase(std::string_view key) {
        return values_.erase(std::string{key}) > 0;
    }

    [[nodiscard]] double add(std::string key, double delta) {
        double next_value = delta;
        if (const auto* current = find(key)) {
            if (const auto* number = std::get_if<double>(current)) {
                next_value = *number + delta;
            }
        }
        set(std::move(key), next_value);
        return next_value;
    }

private:
    ScriptStateValueMap values_{};
};

} // namespace game::script
