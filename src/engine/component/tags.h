#pragma once

namespace engine::component {

/// 不可见标签 —— RenderSystem 跳过渲染
struct InvisibleTag {};

/// 光源禁用标签 —— LightSystem 跳过光照提交，不影响精灵渲染
struct LightDisabledTag {};

/// 延迟移除标签 —— RemoveEntitySystem 在帧末统一销毁
struct NeedRemoveTag {};

/// 动态空间索引标签 —— SpatialIndexSystem 更新动态网格
struct SpatialIndexTag {};

/// 变换脏标签 —— 触发碰撞器更新与空间索引刷新
struct TransformDirtyTag {};

/// 自动图块脏标签 —— AutoTileSystem 重新计算邻接掩码
struct AutoTileDirtyTag {};

} // namespace engine::component
