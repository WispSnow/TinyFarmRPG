# 2026-02-22 Phase 6 逻辑/渲染分离计划（Deferred）

## 元信息
- 阶段：`Phase 6`
- 主题：`逻辑线程与渲染线程分离`
- 优先级：`Optional/不推荐`
- 状态：`Deferred`
- 范围边界：
  - 当前阶段不执行，只保留在“渲染确定成为瓶颈”时的应急方案。
  - 不在当前 2D 农场 RPG 负载下提前引入双线程渲染架构。

## 1. 实现思路（最优方案）

若必须实施，采用 **“固定逻辑帧 + RenderSnapshot 三缓冲”**：

1. 逻辑线程以 fixed timestep 更新 ECS，生成只读 `RenderSnapshot`。
2. 渲染线程消费最近可用快照并执行渲染，不回写游戏状态。
3. 使用三缓冲减少生产者/消费者互斥等待，降低撕裂与卡顿风险。

当前不推荐实施的原因：
- 渲染并非当前瓶颈，复杂度和维护成本远高于收益。
- 快照构建会增加内存与拷贝成本，并引入至少 1 帧输入显示延迟。
- 调试复杂度显著增加（跨线程时序与竞态定位困难）。

## 2. 需要新增的文件（触发实施后）

- `src/game/render/render_snapshot.h`
- `src/game/render/render_snapshot_builder.h`
- `src/game/render/render_snapshot_builder.cpp`
- `src/game/render/render_snapshot_ring_buffer.h`
- `src/game/render/render_snapshot_ring_buffer.cpp`
- `tests/game/render_snapshot_pipeline_test.cpp`

## 预计修改的文件（触发实施后）

- `src/engine/core/game_app.h`
- `src/engine/core/game_app.cpp`
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/game/scene/game_scene.cpp`

## 3. 实现步骤（触发后执行）

### Step 1：性能门槛确认
- 先确认瓶颈位于渲染线程而非 IO/逻辑线程，并建立稳定复现样例。

### Step 2：定义 Snapshot 契约
- 明确快照最小字段（sprites/lights/camera/ui/插值参数）。
- 规定“渲染线程只读快照，不访问 `registry`”。

### Step 3：双线程循环接线
- 将 `GameApp` 循环拆分为逻辑更新与渲染消费两条链路。
- 引入三缓冲与生命周期管理，处理退出、暂停与切场景时序。

### Step 4：验证与回退开关
- 增加开关（默认关闭）以便 A/B 验证。
- 对比延迟、帧时间抖动、输入响应与稳定性。

## 4. 待办清单（用于后续追踪）

- [ ] T1 完成渲染瓶颈证据采集（固定场景 + 性能数据）
- [ ] T2 设计并评审 `RenderSnapshot` 字段契约
- [ ] T3 完成快照三缓冲通道
- [ ] T4 完成 `GameApp` 双线程循环接线
- [ ] T5 新增渲染分离回归测试与稳定性测试
- [ ] T6 增加默认关闭的运行时开关并完成 A/B 基准对比

## 5. 疑问与待澄清
- 暂无。保持 Deferred，除非 profiling 明确指向渲染瓶颈。
