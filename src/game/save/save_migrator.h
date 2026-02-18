#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>

namespace game::save {

/// 将原始存档 JSON 就地迁移到最新 schema。
/// 成功返回 true；失败时 out_error 包含诊断信息。
[[nodiscard]] bool migrateToLatest(nlohmann::json& json, std::string& out_error);

} // namespace game::save
