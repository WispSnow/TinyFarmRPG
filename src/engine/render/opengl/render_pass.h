#pragma once

namespace engine::render::opengl {

class ShaderLibrary;

/**
 * @brief 渲染 pass 的最小统一契约。
 *
 * 当前阶段先统一生命周期清理接口，便于后续做 pass 动态编排；
 * 具体执行/清屏/尺寸调整接口保留在各 pass 内逐步收敛。
 */
class RenderPass {
public:
    virtual ~RenderPass() = default;
    virtual void clean() = 0;
};

/**
 * @brief 支持着色器热重载的渲染 pass 契约。
 */
class ReloadableRenderPass : public RenderPass {
public:
    ~ReloadableRenderPass() override = default;
    [[nodiscard]] virtual bool reload(ShaderLibrary& library) = 0;
};

} // namespace engine::render::opengl
