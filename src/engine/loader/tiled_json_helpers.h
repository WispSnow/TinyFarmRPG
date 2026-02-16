#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace engine::loader::tiled {

[[nodiscard]] inline const nlohmann::json* findMember(const nlohmann::json& obj, const char* key) {
    if (!obj.is_object()) {
        return nullptr;
    }
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return &(*it);
}

[[nodiscard]] inline const nlohmann::json::string_t* findStringMember(const nlohmann::json& obj, const char* key) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return nullptr;
    }
    return value->get_ptr<const nlohmann::json::string_t*>();
}

[[nodiscard]] inline std::string jsonStringOr(const nlohmann::json& obj, const char* key, std::string_view fallback) {
    if (const auto* value = findStringMember(obj, key)) {
        return std::string(*value);
    }
    return std::string(fallback);
}

[[nodiscard]] inline bool jsonBoolOr(const nlohmann::json& obj, const char* key, bool fallback) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return fallback;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::boolean_t*>()) {
        return *v;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_integer_t*>()) {
        return *v != 0;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return *v != 0;
    }
    return fallback;
}

[[nodiscard]] inline int jsonIntOr(const nlohmann::json& obj, const char* key, int fallback) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return fallback;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_integer_t*>()) {
        if (*v < std::numeric_limits<int>::min() || *v > std::numeric_limits<int>::max()) {
            return fallback;
        }
        return static_cast<int>(*v);
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        const auto max_int = static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<int>::max());
        if (*v > max_int) {
            return fallback;
        }
        return static_cast<int>(*v);
    }
    return fallback;
}

[[nodiscard]] inline int jsonGidValueOr(const nlohmann::json& value, int fallback) {
    static_assert(sizeof(int) >= sizeof(std::int32_t), "This engine expects int to fit a 32-bit GID");

    std::uint32_t gid_raw = 0u;
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        if (*v > std::numeric_limits<std::uint32_t>::max()) {
            return fallback;
        }
        gid_raw = static_cast<std::uint32_t>(*v);
    } else if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        if (*v < 0) {
            return fallback;
        }
        const auto max_gid =
            static_cast<nlohmann::json::number_integer_t>(std::numeric_limits<std::uint32_t>::max());
        if (*v > max_gid) {
            return fallback;
        }
        gid_raw = static_cast<std::uint32_t>(*v);
    } else {
        return fallback;
    }

    const auto gid_signed = std::bit_cast<std::int32_t>(gid_raw);
    return static_cast<int>(gid_signed);
}

[[nodiscard]] inline int jsonGidOr(const nlohmann::json& obj, const char* key, int fallback) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return fallback;
    }
    return jsonGidValueOr(*value, fallback);
}

[[nodiscard]] inline std::uint64_t jsonUInt64Or(const nlohmann::json& obj, const char* key, std::uint64_t fallback) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return fallback;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return static_cast<std::uint64_t>(*v);
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_integer_t*>()) {
        if (*v < 0) {
            return fallback;
        }
        return static_cast<std::uint64_t>(*v);
    }
    return fallback;
}

[[nodiscard]] inline float jsonFloatOr(const nlohmann::json& obj, const char* key, float fallback) {
    const auto* value = findMember(obj, key);
    if (!value) {
        return fallback;
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_float_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_integer_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return static_cast<float>(*v);
    }
    return fallback;
}

[[nodiscard]] inline std::vector<int> jsonIntVectorOr(const nlohmann::json& obj,
                                                      const char* key,
                                                      std::vector<int> fallback) {
    const auto* value = findMember(obj, key);
    if (!value || !value->is_array()) {
        return fallback;
    }

    std::vector<int> result;
    result.reserve(value->size());
    for (const auto& entry : *value) {
        if (const auto* v = entry.get_ptr<const nlohmann::json::number_integer_t*>()) {
            if (*v < std::numeric_limits<int>::min() || *v > std::numeric_limits<int>::max()) {
                return fallback;
            }
            result.push_back(static_cast<int>(*v));
            continue;
        }
        if (const auto* v = entry.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            const auto max_int = static_cast<nlohmann::json::number_unsigned_t>(std::numeric_limits<int>::max());
            if (*v > max_int) {
                return fallback;
            }
            result.push_back(static_cast<int>(*v));
            continue;
        }
        return fallback;
    }
    return result;
}

} // namespace engine::loader::tiled

