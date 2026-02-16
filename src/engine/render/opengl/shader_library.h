#pragma once

/**
 * @brief 着色器程序库的轻量级 RAII 封装类。
 *
 * 负责管理 ShaderProgram 实例的创建、获取和销毁，支持开发过程中的热重载功能。
 */

#include "shader_program.h"
#include <memory>
#include <string_view>
#include <unordered_map>
#include <entt/core/fwd.hpp>

namespace engine::render::opengl {

class ShaderLibrary {
    std::unordered_map<entt::id_type, std::unique_ptr<ShaderProgram>> programs_;

public:
    ShaderLibrary() = default;
    ~ShaderLibrary();
    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;
    ShaderLibrary(ShaderLibrary&& other) = delete;
    ShaderLibrary& operator=(ShaderLibrary&& other) = delete;

    /** 
    * @brief 加载着色器程序 
    * @param id 着色器程序的唯一标识符
    * @param vertex_path 顶点着色器文件路径
    * @param fragment_path 片段着色器文件路径
    * @return 加载的着色器程序的指针, 失败返回nullptr
    * @note 如果着色器程序已经加载，则直接返回已加载的着色器程序
    */
    ShaderProgram* loadProgram(entt::id_type id,
                               std::string_view vertex_path,
                               std::string_view fragment_path);

    ShaderProgram* getProgram(entt::id_type id) const;

    bool reloadProgram(entt::id_type id);
    void destroyProgram(entt::id_type id);
    void clear();
};

} // namespace engine::render::opengl
