# TinyFarmRPG 多线程架构分析与建议

## 当前架构分析

项目当前是 **100% 单线程**。没有任何 `std::thread`、`std::mutex`、`std::atomic` 或 `std::async` 的使用。所有系统在主线程上按固定顺序串行执行：

```
主线程循环:
  SDL_PollEvent()          ─┐
  dispatchActionCallbacks() │  Input
  consumeTick()            ─┘
  SystemScheduler::tick()  ─── 22个系统串行执行 (fixedUpdate 60Hz)
  Scene::update()          ─── 变步长更新
  GLRenderer::render()     ─── 多通道渲染 (Scene→Light→Emissive→Bloom→Composite→UI)
  dispatcher_->update()    ─── 延迟事件处理
  SDL_GL_SwapWindow()      ─── VSync等待
```

### 多线程的硬约束

| 约束 | 原因 |
|------|------|
| **SDL3 事件必须在主线程** | `SDL_PollEvent()` 要求主线程调用 |
| **OpenGL 上下文线程绑定** | GL 调用必须在创建上下文的线程上执行（或显式 `MakeCurrent`） |
| **EnTT registry 非线程安全** | 并发读写同一 registry 是未定义行为 |
| **entt::dispatcher 非线程安全** | `enqueue`/`update`/`trigger` 不能并发调用 |
| **ImGui 单线程** | 调试 UI 框架要求单线程访问 |

---

## 建议方案（按投入产出比排序）

### 方案 1: 异步资源加载 (推荐优先实施)

**收益**: 高 | **风险**: 低 | **复杂度**: 中

当前 `ResourceManager` 在主线程同步加载纹理/音频/字体，加载地图时会产生明显卡顿。

```
┌─────────────┐     ┌──────────────────┐
│  主线程      │     │  资源加载线程     │
│             │     │                  │
│  请求加载 ──────→  │  磁盘读取         │
│             │     │  图片解码(stb)    │
│  继续游戏   │     │  音频解码         │
│  ...        │     │  字体栅格化       │
│             │  ←──── 通知完成         │
│  GL上传纹理  │     │                  │
│  (主线程)   │     │                  │
└─────────────┘     └──────────────────┘
```

#### 核心设计

```cpp
// 现代C++线程安全队列
template<typename T>
class ThreadSafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
public:
    void push(T item);
    std::optional<T> try_pop();
    T wait_and_pop();
};

// 异步加载请求
struct LoadRequest {
    entt::id_type asset_id;
    std::filesystem::path path;
    AssetType type;  // Texture, Audio, Font
};

// 加载结果 (CPU端数据, 不含GL句柄)
struct LoadResult {
    entt::id_type asset_id;
    AssetType type;
    std::variant<ImageData, AudioData, FontData> data;
};

class AsyncResourceLoader {
    std::jthread worker_;                     // C++20 自动join
    ThreadSafeQueue<LoadRequest> requests_;
    ThreadSafeQueue<LoadResult> results_;
    std::stop_token stop_token_;

public:
    void requestLoad(LoadRequest req);

    // 主线程每帧调用, 将CPU数据上传到GPU
    void processCompleted(ResourceManager& rm, int max_per_frame = 2);
};
```

#### 关键点

- 磁盘 I/O + 解码在工作线程
- `glTexImage2D` 等 GL 调用仍在主线程（通过 `processCompleted` 分帧上传）
- 使用 `std::jthread` + `std::stop_token` (C++20) 实现优雅关闭
- 场景切换时的地图预加载可以完全异步化

#### 需要修改的文件

- `src/engine/resource/resource_manager.h` — 添加异步接口
- `src/engine/resource/texture_manager.h` — 拆分为 decode (线程安全) + upload (主线程)
- `src/engine/core/game_app.cpp` — 每帧调用 `processCompleted()`
- `src/game/world/map_manager.h` — 地图预加载改为异步

---

### 方案 2: 后台存档 I/O

**收益**: 中 | **风险**: 低 | **复杂度**: 低

`SaveService` 的序列化和磁盘写入可以在后台线程完成。

```cpp
class AsyncSaveService {
    std::jthread save_thread_;
    std::future<bool> pending_save_;

public:
    // 在主线程快照数据, 然后后台写入磁盘
    std::future<bool> saveAsync(const SaveData& snapshot) {
        return std::async(std::launch::async, [snapshot = snapshot]() {
            // JSON序列化 + 文件写入 (纯CPU, 无ECS访问)
            return writeToFile(snapshot);
        });
    }

    bool isSaving() const {
        return pending_save_.valid() &&
               pending_save_.wait_for(0s) != std::future_status::ready;
    }
};
```

#### 关键点

主线程负责从 registry 快照数据（这一步很快），后台线程只做序列化+写盘。

---

### 方案 3: 逻辑/渲染分离 (双缓冲)

**收益**: 中高 | **风险**: 中高 | **复杂度**: 高

将游戏循环拆为两个线程：逻辑线程和渲染线程。

```
逻辑线程 (60Hz固定步长)          渲染线程 (VSync/不限帧率)
┌─────────────────────┐        ┌─────────────────────┐
│ SDL_PollEvent()     │        │                     │
│ Input dispatch      │        │ 等待新的渲染快照     │
│ SystemScheduler     │        │   ↓                 │
│ dispatcher.update() │        │ YSort               │
│   ↓                 │        │ RenderSystem        │
│ 生成渲染快照 ─────────────→   │ LightSystem         │
│   ↓                 │        │ BloomPass           │
│ 继续下一逻辑帧      │        │ CompositePass       │
│                     │        │ UIPass              │
│                     │        │ SDL_GL_SwapWindow() │
└─────────────────────┘        └─────────────────────┘
```

#### 渲染快照

```cpp
struct RenderSnapshot {
    // 从 registry 提取的只读渲染数据
    std::vector<SpriteRenderData> sprites;  // transform + sprite + render
    std::vector<LightRenderData> lights;    // 光源数据
    float interpolation_alpha;
    glm::mat4 view_projection;
    // ... 其他渲染所需数据
};
```

#### 不建议现阶段实施此方案, 原因

1. 渲染管线已经是多通道的（Scene/Light/Bloom/Composite/UI），架构复杂度已经很高
2. 需要维护双缓冲快照，增加内存占用和数据复制开销
3. 2D 农场 RPG 的渲染负载通常不是瓶颈
4. 引入帧延迟（逻辑帧领先渲染帧 1 帧），对输入延迟敏感的交互会有影响
5. 调试难度显著增加

---

### 方案 4: 基于 Task 的 ECS 并行（长期方向）

**收益**: 中 | **风险**: 高 | **复杂度**: 高

分析 `SchedulerStage`，部分系统存在并行可能：

```
Phase 1 (串行): RemoveEntity → TransitionPre → LightTogglePre
Phase 2 (串行): Time → DayNight
Phase 3 (可并行): PlayerControl | NPCWander | AnimalBehavior    ← 不同实体集合
Phase 4 (串行): Chest → ItemUse → Dialogue
Phase 5 (可并行): ActionSound | AutoTile | State                ← 较低耦合
Phase 6 (串行): Movement → TransitionPost → LightTogglePost
Phase 7 (串行): SpatialIndex → Pickup → Interaction
Phase 8 (可并行): CameraFollow | Animation                      ← 独立输出
```

#### 实际并行收益有限, 因为

- EnTT `registry.view()` 的并发读是安全的，但**写操作不安全** — 而大多数系统都写组件
- `NPCWander` 和 `AnimalBehavior` 虽然处理不同实体，但它们都写 `VelocityComponent`、`StateComponent`，底层存储是共享的
- 系统粒度太细（每个系统只处理几十个实体），线程调度开销可能大于并行收益

#### 如果要走这条路

建议使用 EnTT 的 `entt::organizer` 或引入 task graph 库（如 taskflow）:

```cpp
// 概念性示例
tf::Taskflow taskflow;
auto player = taskflow.emplace([&]{ player_control_system->update(dt); });
auto npc    = taskflow.emplace([&]{ npc_wander_system->update(dt); });
auto animal = taskflow.emplace([&]{ animal_behavior_system->update(dt); });
auto movement = taskflow.emplace([&]{ movement_system->update(registry, dt); });

// 声明依赖
movement.succeed(player, npc, animal);

executor.run(taskflow).wait();
```

---

## 总结建议

| 优先级 | 方案 | 建议 |
|-------|------|------|
| **P0** | 异步资源加载 | **立即实施**。收益最高、风险最低。消除地图切换卡顿 |
| **P1** | 后台存档 I/O | **可同时实施**。简单的 `std::async` 就能搞定 |
| **P2** | ECS Task 并行 | **暂不实施**。当实体数量达到数千级别再考虑 |
| **P3** | 逻辑/渲染分离 | **不建议**。对2D农场RPG来说过度工程化 |

**C++ 标准建议**: 至少使用 C++20（`std::jthread`, `std::stop_token`, `std::latch`, `std::barrier`）。如果能用 C++23 更好（`std::expected` 处理异步错误）。
