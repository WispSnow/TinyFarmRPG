// NOLINTBEGIN
#include <gtest/gtest.h>

#include <type_traits>

#include "engine/render/opengl/bloom_pass.h"
#include "engine/render/opengl/composite_pass.h"
#include "engine/render/opengl/emissive_pass.h"
#include "engine/render/opengl/lighting_pass.h"
#include "engine/render/opengl/render_pass.h"
#include "engine/render/opengl/scene_pass.h"
#include "engine/render/opengl/vfx_pass.h"
#include "engine/render/opengl/world_vfx_pass.h"

namespace engine::render::opengl {
namespace {

TEST(RenderPassInterfaceTest, PassTypesFollowUnifiedContracts) {
    EXPECT_TRUE((std::is_base_of_v<RenderPass, ScenePass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, LightingPass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, EmissivePass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, BloomPass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, CompositePass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, WorldVfxPass>));
    EXPECT_TRUE((std::is_base_of_v<RenderPass, VfxPass>));

    EXPECT_TRUE((std::is_base_of_v<ReloadableRenderPass, ScenePass>));
    EXPECT_TRUE((std::is_base_of_v<ReloadableRenderPass, EmissivePass>));
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
