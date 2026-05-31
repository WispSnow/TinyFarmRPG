#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace engine::utils::json {

[[nodiscard]] inline const nlohmann::json* findMember(const nlohmann::json& object, std::string_view key) {
    if (!object.is_object()) {
        return nullptr;
    }
    const auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return &(*it);
}

[[nodiscard]] inline bool readString(const nlohmann::json& value, std::string& out_value) {
    if (const auto* text = value.get_ptr<const nlohmann::json::string_t*>()) {
        out_value = *text;
        return true;
    }
    return false;
}

template <typename T>
[[nodiscard]] bool readNumber(const nlohmann::json& value, T& out_value) {
    if constexpr (std::is_floating_point_v<T>) {
        if (const auto* number = value.get_ptr<const nlohmann::json::number_float_t*>()) {
            out_value = static_cast<T>(*number);
            return true;
        }
        if (const auto* number = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
            out_value = static_cast<T>(*number);
            return true;
        }
        if (const auto* number = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            out_value = static_cast<T>(*number);
            return true;
        }
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        if (const auto* number = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
            if (*number < static_cast<nlohmann::json::number_integer_t>(std::numeric_limits<T>::min()) ||
                *number > static_cast<nlohmann::json::number_integer_t>(std::numeric_limits<T>::max())) {
                return false;
            }
            out_value = static_cast<T>(*number);
            return true;
        }
        if (const auto* number = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            if (*number > static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<T>::max())) {
                return false;
            }
            out_value = static_cast<T>(*number);
            return true;
        }
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        if (const auto* number = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            if (*number > static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<T>::max())) {
                return false;
            }
            out_value = static_cast<T>(*number);
            return true;
        }
        if (const auto* number = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
            if (*number < 0) {
                return false;
            }
            const auto unsigned_number = static_cast<nlohmann::json::number_unsigned_t>(*number);
            if (unsigned_number > static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<T>::max())) {
                return false;
            }
            out_value = static_cast<T>(unsigned_number);
            return true;
        }
    }
    return false;
}

template <typename T>
[[nodiscard]] T numberOr(const nlohmann::json& object, std::string_view key, T fallback) {
    const auto* value = findMember(object, key);
    if (!value) {
        return fallback;
    }
    T result = fallback;
    return readNumber(*value, result) ? result : fallback;
}

[[nodiscard]] inline bool boolOr(const nlohmann::json& object, std::string_view key, bool fallback) {
    const auto* value = findMember(object, key);
    if (!value) {
        return fallback;
    }
    if (const auto* flag = value->get_ptr<const nlohmann::json::boolean_t*>()) {
        return *flag;
    }
    return fallback;
}

[[nodiscard]] inline std::string stringOr(const nlohmann::json& object,
                                          std::string_view key,
                                          std::string_view fallback) {
    const auto* value = findMember(object, key);
    if (!value) {
        return std::string(fallback);
    }
    std::string result{};
    return readString(*value, result) ? result : std::string(fallback);
}

} // namespace engine::utils::json
