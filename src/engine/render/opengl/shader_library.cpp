// -----------------------------------------------------------------------------
// ShaderLibrary.cpp
// -----------------------------------------------------------------------------
// 一个简单的缓存，使用标识符作为键，存储 ShaderProgram 实例。
// 渲染器依赖此缓存，避免每次渲染通道使用时重新编译着色器，并支持开发过程中的热重载功能。
// -----------------------------------------------------------------------------
#include "shader_library.h"
#include "shader_program.h"
#include <memory>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace engine::render::opengl {

ShaderLibrary::~ShaderLibrary() {
    clear();
}

ShaderProgram* ShaderLibrary::loadProgram(entt::id_type id,
                                          std::string_view vertex_path,
                                          std::string_view fragment_path) {
    // 若该 id 已存在，则直接复用，避免替换导致外部持有的裸指针失效
    auto it = programs_.find(id);
    if (it != programs_.end()) {
        return it->second.get();
    }

    auto program = ShaderProgram::create(vertex_path, fragment_path);
    if (!program) {
        spdlog::error("加载着色器失败: id = {}, vertex_path = {}, fragment_path = {}", id, vertex_path, fragment_path);
        return nullptr;
    }

    programs_.emplace(id, std::move(program));
    return programs_.at(id).get();
}

ShaderProgram* ShaderLibrary::getProgram(entt::id_type id) const {
    auto it = programs_.find(id);
    if (it == programs_.end()) {
        spdlog::error("着色器未找到: id = {}", id);
        return nullptr;
    }
    return it->second.get();
}

bool ShaderLibrary::reloadProgram(entt::id_type id) {
    ShaderProgram* program = getProgram(id);
    if (!program) {
        return false;
    }
    return program->reload();
}

void ShaderLibrary::destroyProgram(entt::id_type id) {
    auto it = programs_.find(id);
    if (it != programs_.end()) {
        programs_.erase(it);
    }
}

void ShaderLibrary::clear() {
    programs_.clear();
}

} // namespace engine::render::opengl
