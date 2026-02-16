#pragma once

/**
 * @brief OpenGL 着色器程序的轻量级 RAII 封装类。
 *
 * 该类负责编译顶点和片段着色器（支持源码或文件路径），统一变量查询缓存，并支持移动语义，
 * 便于在如 ShaderLibrary 等容器中安全管理，避免内存泄漏。
 */

#include <glad/glad.h>
#include <string>
#include <string_view>
#include <memory>

namespace engine::render::opengl {

class ShaderProgram final{
    GLuint program_{0};
    std::string vertex_path_;
    std::string fragment_path_;

public:
    [[nodiscard]] static std::unique_ptr<ShaderProgram> create(std::string_view vertex_path,
                                                               std::string_view fragment_path);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    bool reload();

    void clean();

    void use() const { glUseProgram(program_); }
    void unuse() const { glUseProgram(0); }
    GLuint program() const { return program_; }

    GLint uniformLocation(std::string_view name) const;

    std::string_view vertexPath() const { return vertex_path_; }
    std::string_view fragmentPath() const { return fragment_path_; }

private:
    ShaderProgram() = default;

    bool loadFromFiles(std::string_view vertex_path,
                       std::string_view fragment_path);

    bool loadFromSources(std::string_view vertex_source,
                         std::string_view fragment_source);

    bool linkProgram(GLuint vertex_shader, GLuint fragment_shader);
    GLuint compileShader(GLenum type, std::string_view source) const;
};

} // namespace engine::render::opengl
