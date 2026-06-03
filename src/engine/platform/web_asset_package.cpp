#include "web_asset_package.h"

#include <spdlog/spdlog.h>

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
#include <emscripten.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#endif

namespace engine::platform::web {
namespace {

#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
constexpr std::array<char, 8> PACKAGE_MAGIC{'T', 'F', 'P', 'K', '0', '0', '0', '1'};
constexpr std::size_t PACKAGE_HEADER_SIZE_OFFSET = PACKAGE_MAGIC.size();
constexpr std::size_t PACKAGE_DATA_OFFSET_BASE = PACKAGE_MAGIC.size() + sizeof(std::uint32_t);

extern "C" {
EM_JS(int, tf_web_sync_load_binary_package, (const char* url_ptr, unsigned char** out_data, unsigned int* out_size), {
    const url = UTF8ToString(url_ptr);
    try {
        const request = new XMLHttpRequest();
        request.open("GET", url, false);
        request.overrideMimeType("text/plain; charset=x-user-defined");
        request.send(null);

        if ((request.status !== 200 && request.status !== 0) || !request.responseText) {
            console.error("TinyFarmRPG package request failed: " + url + " status=" + request.status);
            return request.status || -1;
        }

        const text = request.responseText;
        const size = text.length;
        const data = _malloc(size);
        if (!data) {
            console.error("TinyFarmRPG package allocation failed: " + url + " bytes=" + size);
            return -2;
        }

        for (let index = 0; index < size; ++index) {
            HEAPU8[data + index] = text.charCodeAt(index) & 0xff;
        }
        HEAPU32[out_data >> 2] = data;
        HEAPU32[out_size >> 2] = size;
        return 0;
    } catch (error) {
        console.error("TinyFarmRPG package request threw: " + url, error);
        return -3;
    }
});
}

using PackageBuffer = std::unique_ptr<unsigned char, decltype(&std::free)>;

struct LoadedPackageData {
    PackageBuffer bytes{nullptr, &std::free};
    std::uint32_t size{0};
};

std::unordered_set<std::string>& loadedPackages() {
    static std::unordered_set<std::string> packages{};
    return packages;
}

[[nodiscard]] std::uint32_t readU32Le(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8U)
         | (static_cast<std::uint32_t>(bytes[2]) << 16U)
         | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

[[nodiscard]] const nlohmann::json* findMember(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) {
        return nullptr;
    }
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &(*it);
}

[[nodiscard]] std::string stringMemberOrEmpty(const nlohmann::json& object, const char* key) {
    const auto* value = findMember(object, key);
    if (!value || !value->is_string()) {
        return {};
    }
    return value->get<std::string>();
}

[[nodiscard]] std::uint64_t unsignedMemberOrZero(const nlohmann::json& object, const char* key) {
    const auto* value = findMember(object, key);
    if (!value || !value->is_number_unsigned()) {
        return 0;
    }
    return value->get<std::uint64_t>();
}

[[nodiscard]] bool writePackageFile(const std::filesystem::path& path,
                                    const char* data,
                                    std::uint64_t size) {
    std::error_code ec{};
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            spdlog::error("WebAssetPackage: 无法创建目录 '{}': {}", parent.string(), ec.message());
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        spdlog::error("WebAssetPackage: 无法写入包文件 '{}'", path.string());
        return false;
    }
    out.write(data, static_cast<std::streamsize>(size));
    return out.good();
}

[[nodiscard]] bool installPackageData(std::string_view package_id, const char* bytes, std::uint64_t byte_count) {
    if (byte_count < PACKAGE_DATA_OFFSET_BASE) {
        spdlog::error("WebAssetPackage: package '{}' 太小，无法读取头部。", package_id);
        return false;
    }

    if (std::memcmp(bytes, PACKAGE_MAGIC.data(), PACKAGE_MAGIC.size()) != 0) {
        spdlog::error("WebAssetPackage: package '{}' magic 无效。", package_id);
        return false;
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(bytes);
    const std::uint32_t header_size = readU32Le(raw + PACKAGE_HEADER_SIZE_OFFSET);
    const std::uint64_t data_start = PACKAGE_DATA_OFFSET_BASE + static_cast<std::uint64_t>(header_size);
    if (data_start > byte_count) {
        spdlog::error("WebAssetPackage: package '{}' header 越界。", package_id);
        return false;
    }

    const std::string header_text(bytes + PACKAGE_DATA_OFFSET_BASE, header_size);
    const nlohmann::json header = nlohmann::json::parse(header_text, nullptr, false);
    if (header.is_discarded() || !header.is_object()) {
        spdlog::error("WebAssetPackage: package '{}' header JSON 无效。", package_id);
        return false;
    }

    const std::string header_package_id = stringMemberOrEmpty(header, "package_id");
    if (header_package_id != package_id) {
        spdlog::error(
            "WebAssetPackage: package id 不匹配，期望 '{}'，实际 '{}'",
            package_id,
            header_package_id);
        return false;
    }

    const auto* files = findMember(header, "files");
    if (!files || !files->is_array() || files->empty()) {
        spdlog::error("WebAssetPackage: package '{}' 不包含文件。", package_id);
        return false;
    }

    std::size_t installed = 0;
    for (const auto& file : *files) {
        if (!file.is_object()) {
            spdlog::error("WebAssetPackage: package '{}' 文件条目格式无效。", package_id);
            return false;
        }

        const std::string path = stringMemberOrEmpty(file, "path");
        const std::uint64_t offset = unsignedMemberOrZero(file, "offset");
        const std::uint64_t size = unsignedMemberOrZero(file, "size");
        if (path.empty() || !path.starts_with('/')) {
            spdlog::error("WebAssetPackage: package '{}' 文件路径无效: '{}'", package_id, path);
            return false;
        }
        if (data_start + offset + size > byte_count) {
            spdlog::error("WebAssetPackage: package '{}' 文件 '{}' 越界。", package_id, path);
            return false;
        }
        if (!writePackageFile(std::filesystem::path{path}, bytes + data_start + offset, size)) {
            return false;
        }
        ++installed;
    }

    spdlog::info("WebAssetPackage: package '{}' loaded ({} files).", package_id, installed);
    return true;
}

[[nodiscard]] LoadedPackageData loadPackageData(std::string_view package_id, std::string_view package_url) {
    std::string url{package_url};
    unsigned char* raw_data = nullptr;
    unsigned int raw_size = 0;
    const int result = tf_web_sync_load_binary_package(url.c_str(), &raw_data, &raw_size);
    if (result != 0 || raw_data == nullptr || raw_size == 0) {
        if (raw_data != nullptr) {
            std::free(raw_data);
        }
        spdlog::error(
            "WebAssetPackage: package '{}' 同步 XHR 加载失败 url='{}' result={} bytes={}",
            package_id,
            package_url,
            result,
            raw_size);
        return {};
    }
    return LoadedPackageData{PackageBuffer{raw_data, &std::free}, static_cast<std::uint32_t>(raw_size)};
}
#endif

} // namespace

bool loadAssetPackage(std::string_view package_id, std::string_view package_url) {
#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    auto& loaded = loadedPackages();
    const std::string id{package_id};
    if (loaded.contains(id)) {
        return true;
    }

    const LoadedPackageData package_data = loadPackageData(package_id, package_url);
    if (!package_data.bytes || package_data.size == 0) {
        return false;
    }

    if (!installPackageData(
            package_id,
            reinterpret_cast<const char*>(package_data.bytes.get()),
            static_cast<std::uint64_t>(package_data.size))) {
        return false;
    }

    loaded.insert(id);
    return true;
#else
    (void)package_id;
    (void)package_url;
    return true;
#endif
}

bool isAssetPackageLoaded(std::string_view package_id) {
#if defined(__EMSCRIPTEN__) && defined(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    return loadedPackages().contains(std::string{package_id});
#else
    (void)package_id;
    return true;
#endif
}

} // namespace engine::platform::web
