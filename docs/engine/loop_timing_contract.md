# 逻辑循环 / 渲染循环拆分契约（Step 1）

> 日期：2026-02-20  
> 适用范围：`GameApp` 主循环、`Time`、`InputManager`、`Scene/SceneManager`、`GameScene`

## 1. 目标与非目标

### 1.1 目标
- 逻辑更新采用固定步长（fixed timestep）。
- 渲染采用可变步长（as fast as possible，可选限速）。
- 保持现有事件语义兼容（尤其 `dispatcher.update()` 时机）。

### 1.2 非目标
- 本阶段不强制引入全量组件插值。
- 不改变 Scene 栈“仅更新栈顶、渲染全栈”的核心规则。

## 2. 术语与默认参数

- `fixed_dt`：固定逻辑步长，默认 `1 / 60` 秒。
- `frame_delta`：相邻渲染帧真实时间差（未缩放）。
- `time_scale`：时间缩放系数，默认 `1.0`。
- `scaled_frame_delta = frame_delta * time_scale`。
- `accumulator`：逻辑时间累积器。
- `max_ticks_per_frame`：单渲染帧最大逻辑 tick 数，默认 `5`。

## 3. 核心语义约束

### 3.1 固定步长不被 time_scale 改写
- `fixed_dt` 是常量，不随 `time_scale` 改变。
- `time_scale` 仅影响 `accumulator` 的输入（即逻辑 tick 的触发频率），不影响每个 tick 的步长值。

### 3.2 dispatcher 刷新时机保持兼容
- `dispatcher.update()` 保持“每个渲染帧一次，且在 render 之后”。
- 不改为“每个逻辑 tick 一次”，避免破坏现有 enqueue 事件的帧尾结算语义。

### 3.3 catch-up 防爆帧
- 每个渲染帧最多执行 `max_ticks_per_frame` 次逻辑 tick（默认 5）。
- 若达到上限后 `accumulator` 仍超额，丢弃超额 backlog，并记录统计（用于调试面板）。

### 3.4 场景切换清空残余累积
- 一旦检测到场景栈结构变化（push/pop/replace 生效），清空 `accumulator`，防止新场景收到残余时间导致的 tick 爆发。

## 4. 主循环阶段顺序（契约版）

每个渲染帧按以下阶段执行：

1. 采样输入事件（一次）
2. 采样 `frame_delta` 并写入 `accumulator`（按 `time_scale` 缩放）
3. 固定逻辑循环（最多 `max_ticks_per_frame` 次）  
   - 每次使用 `fixed_dt`  
   - 执行 fixed update 路径  
   - 每次 tick 后检查是否发生场景切换，若发生则清空 `accumulator`
4. 执行帧表现更新（frame update）
5. 渲染（render）
6. `dispatcher.update()`（帧尾一次）

## 5. Scene 分层契约

- Scene 提供两类更新入口：
  - `fixedUpdate(fixed_dt)`：固定逻辑更新（默认空实现）
  - `update(frame_dt)`：帧表现更新
- `render(alpha)`：渲染入口（全栈叠加），`alpha` 为渲染插值系数。
- `GameScene` 实现 `fixedUpdate` 承载 gameplay scheduler。
- `TitleScene`、`PauseMenuScene`、`RestDialogScene` 等非 gameplay 场景默认不实现 `fixedUpdate`，继续走 `update(frame_dt)`。

### 5.1 渲染插值通道（Step 7）
- `alpha` 沿 `GameApp -> SceneManager -> Scene -> GameScene -> Render/Light` 透传。
- `render_interpolation=false` 时，`alpha` 固定为 `1.0`，行为等价于“不插值”。
- 当前插值范围限定在 `Transform(position)` 与相机，不扩散到其他组件。

## 6. 输入语义契约（为 Step 3/4 准备）

- 输入系统拆分为：
  - 每渲染帧一次的事件采样
  - 每逻辑 tick 的状态消费/推进
- 要求：
  - `PRESSED/RELEASED` 边沿在同一渲染帧内不重复触发
  - 多 tick 情况下不提前丢失边沿语义

## 7. 可观测性契约（为 Step 8 准备）

至少暴露以下调试指标：
- 当前 `fixed_dt` / 逻辑 Hz
- 渲染 FPS（估算）
- 每帧执行的逻辑 tick 数
- `accumulator` 值
- catch-up 丢弃计数
- 插值 `alpha`（raw/effective）与 `render_interpolation` 开关状态

## 8. 验收条件（Step 1）

- 契约文档已形成并冻结为后续实现基线。
- 计划文件已引用本契约并将 Step 1 标记为完成。
- 后续步骤（Step 2+）若偏离本契约，需先更新本文件并说明理由。
