#include "game/runtime/localization_service.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace game::runtime {

namespace {

[[nodiscard]] nlohmann::json parseJsonFile(std::string_view path) {
    const std::filesystem::path fs_path{path};
    std::ifstream file(fs_path);
    if (!file.is_open()) {
        spdlog::warn("LocalizationService: 无法打开 '{}'", fs_path.string());
        return {};
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    nlohmann::json root = nlohmann::json::parse(content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::warn("LocalizationService: JSON 解析失败 '{}'", fs_path.string());
        return {};
    }
    return root;
}

[[nodiscard]] std::string stringField(const nlohmann::json& object,
                                      std::string_view field,
                                      std::string_view fallback = {}) {
    const auto it = object.find(std::string{field});
    if (it == object.end() || !it->is_string()) {
        return std::string{fallback};
    }
    return it->get<std::string>();
}

[[nodiscard]] std::string resolveLanguageFilePath(std::string_view manifest_path, std::string file_path) {
    const std::filesystem::path raw_path{file_path};
    std::error_code ec{};
    if (raw_path.is_absolute() || std::filesystem::exists(raw_path, ec)) {
        return file_path;
    }

    std::filesystem::path search_root = std::filesystem::path{manifest_path}.parent_path();
    while (!search_root.empty()) {
        const std::filesystem::path candidate = search_root / raw_path;
        ec.clear();
        if (std::filesystem::exists(candidate, ec)) {
            return candidate.string();
        }
        const std::filesystem::path parent = search_root.parent_path();
        if (parent == search_root) {
            break;
        }
        search_root = parent;
    }
    return file_path;
}

void replaceAll(std::string& value, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) {
        return;
    }

    std::size_t pos = 0;
    while ((pos = value.find(needle, pos)) != std::string::npos) {
        value.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

[[nodiscard]] bool isPlaceholderNameChar(char value) {
    const unsigned char c = static_cast<unsigned char>(value);
    return std::isalnum(c) != 0 || value == '_' || value == '.' || value == '-';
}

[[nodiscard]] bool containsUnresolvedPlaceholder(std::string_view text) {
    std::size_t open = text.find('{');
    while (open != std::string_view::npos) {
        const std::size_t close = text.find('}', open + 1);
        if (close == std::string_view::npos) {
            return false;
        }
        if (close > open + 1) {
            bool valid_name = true;
            for (std::size_t i = open + 1; i < close; ++i) {
                if (!isPlaceholderNameChar(text[i])) {
                    valid_name = false;
                    break;
                }
            }
            if (valid_name) {
                return true;
            }
        }
        open = text.find('{', close + 1);
    }
    return false;
}

[[nodiscard]] bool loadLanguageTable(const LocalizationLanguageInfo& info,
                                     std::unordered_map<std::string, std::string>& out_table) {
    const nlohmann::json root = parseJsonFile(info.file);
    if (!root.is_object()) {
        spdlog::warn("LocalizationService: language file '{}' 不是 object", info.file);
        return false;
    }

    out_table.clear();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.value().is_string()) {
            out_table.emplace(it.key(), it.value().get<std::string>());
        }
    }
    return true;
}

} // namespace

bool LocalizationService::loadLanguageIndex(std::string_view manifest_path) {
    const nlohmann::json root = parseJsonFile(manifest_path);
    if (!root.is_object()) {
        return false;
    }

    const auto languages_it = root.find("languages");
    if (languages_it == root.end() || !languages_it->is_array()) {
        spdlog::warn("LocalizationService: language manifest 缺少 languages array '{}'", manifest_path);
        return false;
    }

    std::vector<LocalizationLanguageInfo> next_languages{};
    for (const auto& entry : *languages_it) {
        if (!entry.is_object()) {
            continue;
        }

        LocalizationLanguageInfo info{};
        info.tag = stringField(entry, "tag");
        info.native_name = stringField(entry, "native_name", info.tag);
        info.file = stringField(entry, "file");
        if (info.tag.empty() || info.file.empty()) {
            spdlog::warn("LocalizationService: language entry 缺少 tag 或 file，已跳过。");
            continue;
        }
        info.file = resolveLanguageFilePath(manifest_path, std::move(info.file));
        next_languages.push_back(std::move(info));
    }

    if (next_languages.empty()) {
        spdlog::warn("LocalizationService: language manifest 未提供任何有效语言 '{}'", manifest_path);
        return false;
    }

    const auto find_next_language = [&next_languages](std::string_view language_tag)
        -> const LocalizationLanguageInfo* {
        for (const auto& info : next_languages) {
            if (info.tag == language_tag) {
                return &info;
            }
        }
        return nullptr;
    };

    const std::string requested_fallback = stringField(root, "fallback", next_languages.front().tag);
    const auto* next_fallback = find_next_language(requested_fallback);
    if (!next_fallback) {
        next_fallback = find_next_language(fallback_language_tag_);
    }
    if (!next_fallback) {
        next_fallback = &next_languages.front();
    }

    std::unordered_map<std::string, std::string> fallback_table{};
    if (!loadLanguageTable(*next_fallback, fallback_table)) {
        spdlog::warn(
            "LocalizationService: fallback language '{}' 加载失败，language manifest 未提交。",
            next_fallback->tag);
        return false;
    }

    std::string next_fallback_tag = next_fallback->tag;
    languages_ = std::move(next_languages);
    tables_.clear();
    tables_.emplace(next_fallback_tag, std::move(fallback_table));
    missing_key_warnings_.clear();
    unresolved_placeholder_warnings_.clear();
    fallback_language_tag_ = std::move(next_fallback_tag);
    current_language_tag_ = fallback_language_tag_;
    return true;
}

bool LocalizationService::setLanguage(std::string_view language_tag) {
    const std::string resolved = resolveSupportedLanguageTag(language_tag);
    const auto* info = findLanguage(resolved);
    if (!info) {
        return false;
    }

    if (!loadLanguageFile(*info)) {
        const auto* fallback = findLanguage(fallback_language_tag_);
        if (!fallback || !loadLanguageFile(*fallback)) {
            return false;
        }
        spdlog::warn(
            "LocalizationService: language '{}' 加载失败，已回退到 '{}'",
            resolved,
            fallback_language_tag_);
        current_language_tag_ = fallback_language_tag_;
        return true;
    }

    current_language_tag_ = resolved;
    return true;
}

std::string LocalizationService::tr(std::string_view key) const {
    if (key.empty()) {
        return {};
    }

    if (const auto* table = tableFor(current_language_tag_)) {
        if (const auto it = table->find(std::string{key}); it != table->end()) {
            return it->second;
        }
    }

    if (current_language_tag_ != fallback_language_tag_) {
        if (const auto* table = tableFor(fallback_language_tag_)) {
            if (const auto it = table->find(std::string{key}); it != table->end()) {
                return it->second;
            }
        }
    }

    warnMissingOnce(key);
    return missingText(key);
}

std::string LocalizationService::format(
    std::string_view key,
    const std::unordered_map<std::string, std::string>& args) const {
    std::string text = tr(key);
    for (const auto& [name, value] : args) {
        replaceAll(text, "{" + name + "}", value);
    }
    if (containsUnresolvedPlaceholder(text)) {
        warnUnresolvedPlaceholdersOnce(key, text);
    }
    return text;
}

bool LocalizationService::hasText(std::string_view key) const {
    if (key.empty()) {
        return false;
    }

    const auto has_in_table = [key](const auto* table) {
        return table && table->find(std::string{key}) != table->end();
    };
    return has_in_table(tableFor(current_language_tag_)) || has_in_table(tableFor(fallback_language_tag_));
}

std::string LocalizationService::resolveSupportedLanguageTag(std::string_view language_tag) const {
    if (const auto* info = findLanguage(language_tag)) {
        return info->tag;
    }
    if (const auto* fallback = findLanguage(fallback_language_tag_)) {
        return fallback->tag;
    }
    return languages_.empty() ? std::string{"en-US"} : languages_.front().tag;
}

bool LocalizationService::isSupportedLanguage(std::string_view language_tag) const {
    return findLanguage(language_tag) != nullptr;
}

std::string LocalizationService::languageNativeName(std::string_view language_tag) const {
    const auto* info = findLanguage(language_tag);
    if (!info) {
        return std::string{language_tag};
    }
    return info->native_name.empty() ? info->tag : info->native_name;
}

const LocalizationLanguageInfo* LocalizationService::findLanguage(std::string_view language_tag) const {
    for (const auto& info : languages_) {
        if (info.tag == language_tag) {
            return &info;
        }
    }
    return nullptr;
}

bool LocalizationService::loadLanguageFile(const LocalizationLanguageInfo& info) {
    if (tables_.find(info.tag) != tables_.end()) {
        return true;
    }

    std::unordered_map<std::string, std::string> table{};
    if (!loadLanguageTable(info, table)) {
        return false;
    }

    tables_.emplace(info.tag, std::move(table));
    return true;
}

const std::unordered_map<std::string, std::string>* LocalizationService::tableFor(
    std::string_view language_tag) const {
    const auto it = tables_.find(std::string{language_tag});
    return it == tables_.end() ? nullptr : &it->second;
}

std::string LocalizationService::missingText(std::string_view key) const {
    std::string output{"!"};
    output.append(key.data(), key.size());
    output.push_back('!');
    return output;
}

void LocalizationService::warnMissingOnce(std::string_view key) const {
    const std::string key_text{key};
    if (missing_key_warnings_.insert(key_text).second) {
        spdlog::warn("LocalizationService: 缺失文本 key '{}'", key_text);
    }
}

void LocalizationService::warnUnresolvedPlaceholdersOnce(std::string_view key, std::string_view text) const {
    const std::string key_text{key};
    if (unresolved_placeholder_warnings_.insert(key_text).second) {
        spdlog::warn("LocalizationService: 文本 key '{}' 格式化后仍包含占位符 '{}'", key_text, text);
    }
}

} // namespace game::runtime
