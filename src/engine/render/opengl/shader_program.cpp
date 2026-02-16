// -----------------------------------------------------------------------------
// ShaderProgram.cpp
// -----------------------------------------------------------------------------
// ShaderProgram 包装器的具体实现。
// 负责着色器的编译、链接、错误报告，以及简单的 uniform 查询。
// 所有文件 I/O 操作集中于此，确保渲染器其他部分只需关注 GL 对象的生命周期管理。
// -----------------------------------------------------------------------------
#include "shader_program.h"
#include "gl_helper.h"
#include <fstream>
#include <filesystem>
#include <optional>
#include <spdlog/spdlog.h>

namespace { // anonymous namespace，用于封装辅助函数

std::optional<std::string> readFileText(std::string_view path) {
    auto path_str = std::filesystem::path(path);
    std::ifstream ifs(path_str, std::ios::in | std::ios::binary);
    if (!ifs) {
        spdlog::error("无法打开文件: {}", path_str.string());
        return std::nullopt;
    }
    
    std::string content;
    // seekg(0, std::ios::end) 将文件指针移动到文件末尾，用于获取文件大小
    // tellg() 返回当前文件指针的位置（即末尾位置），等同于文件字节大小
    ifs.seekg(0, std::ios::end); 
    const auto size = ifs.tellg();
    
    // 检查 tellg 是否失败
    if (size < 0) {
        spdlog::error("无法获取文件大小: {}", path_str.string());
        return std::nullopt;
    }
    
    content.reserve(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    
    content.assign(std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>());
    return content;
}

} // anonymous namespace

namespace engine::render::opengl {

std::unique_ptr<ShaderProgram> ShaderProgram::create(std::string_view vertex_path,
                                                      std::string_view fragment_path) {
    auto shader_program = std::unique_ptr<ShaderProgram>(new ShaderProgram());
    if (!shader_program->loadFromFiles(vertex_path, fragment_path)) {
        return nullptr;
    }
    return shader_program;
}

ShaderProgram::~ShaderProgram() {
    clean();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept {
    *this = std::move(other);
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    clean();
    program_ = other.program_;
    vertex_path_ = std::move(other.vertex_path_);
    fragment_path_ = std::move(other.fragment_path_);
    other.program_ = 0;
    return *this;
}

bool ShaderProgram::loadFromFiles(std::string_view vertex_path,
                                  std::string_view fragment_path) {
    auto vertex_source = readFileText(vertex_path);
    if (!vertex_source) {
        spdlog::error("无法读取顶点着色器文件: {}", vertex_path);
        return false;
    }

    auto fragment_source = readFileText(fragment_path);
    if (!fragment_source) {
        spdlog::error("无法读取片段着色器文件: {}", fragment_path);
        return false;
    }

    if (!loadFromSources(*vertex_source, *fragment_source)) {
        spdlog::error("无法加载着色器程序: {} / {}", vertex_path, fragment_path);
        return false;
    }

    // 记住文件路径，以便 reload() 可以获取源代码
    vertex_path_ = vertex_path;
    fragment_path_ = fragment_path;
    return logGlErrors("ShaderProgram::loadFromFiles");
}

bool ShaderProgram::loadFromSources(std::string_view vertex_source,
                                    std::string_view fragment_source) {
    // 编译
    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_source);
    if (vertex_shader == 0) {
        spdlog::error("顶点着色器编译失败");
        return false;
    }
    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment_shader == 0) {
        spdlog::error("片段着色器编译失败");
        glDeleteShader(vertex_shader);
        return false;
    }

    // 链接
    bool linked = linkProgram(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (!linked) {
        spdlog::error("无法链接着色器程序: {} / {}", vertex_path_, fragment_path_);
        return false;
    }

    // 清理
    vertex_path_.clear();
    fragment_path_.clear();
    return logGlErrors("ShaderProgram::loadFromSources");
}

bool ShaderProgram::reload() {
    if (vertex_path_.empty() || fragment_path_.empty()) {
        spdlog::error("ShaderProgram::reload: 调用时没有存储的文件路径");
        return false;
    }
    return loadFromFiles(vertex_path_, fragment_path_);
}

void ShaderProgram::clean() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    vertex_path_.clear();
    fragment_path_.clear();
}

GLint ShaderProgram::uniformLocation(std::string_view name) const {
    if (program_ == 0) {
        spdlog::error("ShaderProgram::uniformLocation: 没有有效的着色器程序");
        return -1;
    }
    GLint location = glGetUniformLocation(program_, name.data());
    if (location == -1) {
        spdlog::error("无法获取 uniform 位置: {}", name.data());
    }
    return location;
}

bool ShaderProgram::linkProgram(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        spdlog::error("无法创建程序: {}", program);
        return false;
    }
    // 将顶点着色器和片段着色器附加到程序
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // 检查程序链接是否成功，如果失败，获取错误日志并删除程序
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        GLsizei written = 0;
        glGetProgramInfoLog(program, logLen, &written, log.data());
        spdlog::error("程序链接错误: {}", log.c_str());
        glDeleteProgram(program);
        return false;
    }

    // 销毁当前程序，并使用新创建的程序
    clean();
    program_ = program;
    return true;
}

GLuint ShaderProgram::compileShader(GLenum type, std::string_view source) const {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        spdlog::error("无法创建着色器: type={}", type);
        return 0;
    }
    // 将着色器源代码传递给 OpenGL, 编译着色器
    const char* src = source.data();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    // 检查着色器编译是否成功
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        GLsizei written = 0;
        glGetShaderInfoLog(shader, logLen, &written, log.data());
        spdlog::error("着色器编译错误: {}", log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace engine::render::opengl
