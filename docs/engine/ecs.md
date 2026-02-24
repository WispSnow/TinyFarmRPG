# ECS 约定：Registry、Component、System、Tag（TinyFarm）

> 用途：统一项目内 ECS 的边界与使用方式，降低“组件放哪 / 系统怎么查 / 数据从哪来”的理解成本。

TinyFarm 使用 EnTT 的 `entt::registry` 作为 ECS 数据容器，并围绕它组织：
- **Entity**：`entt::entity`（只是一个 id）
- **Component**：挂在实体上的数据（纯数据为主）
- **System**：用 view 查询组件组合，并在每帧按顺序更新
- **Tag**：无数据但有语义的组件（marker/dirty/filter）

---

## 1) 核心边界：一个 Scene 一个 registry
本项目的 ECS 边界非常明确：
- `engine::scene::Scene` **拥有**一个 `entt::registry registry_`
- Scene 的生命周期天然是 ECS 状态的生命周期边界：
  - `init()`：初始化场景内状态（创建实体、放入 ctx 数据、创建系统等）
  - `update()/render()`：系统读写 registry 驱动世界
  - `clean()`：清空场景状态（实体/组件）。注意 `registry.clear()` **不会**清除 `registry.ctx()` 中的数据；ctx 数据随 registry 对象销毁（即 Scene 析构）而释放

对应线索：
- `src/engine/scene/scene.h`：`entt::registry registry_`
- `src/engine/scene/scene.cpp`：`Scene::clean()`

---

## 2) Context vs registry：服务与状态分离
理解 TinyFarm 架构的一个关键对照是：
- `engine::core::Context`：**全局服务**（dispatcher、renderer、resource、audio、input、time…），服务对象通常跨 Scene 复用
- `entt::registry`：**场景状态**（实体与组件），随着 Scene 切换而切换/清空

对应线索：
- `src/engine/core/context.h`：Context 内持有各种模块引用
- `src/game/scene/game_scene.cpp`：GameScene 在 `update()` 中驱动 Systems，并通过 Context 调用渲染/输入等服务

---

## 3) Component 分层：engine 通用 vs game 语义
组件按“通用能力/玩法语义”分层，避免把游戏语义塞进引擎层：

- `src/engine/component/*`：引擎通用组件（可被多个游戏/场景复用）
  - 例：`TransformComponent`、`SpriteComponent`、`RenderComponent`、`VelocityComponent`
  - 例：碰撞与光照：`AABBCollider/CircleCollider`、`PointLight/SpotLight` 等

- `src/game/component/*`：游戏语义组件（只属于 TinyFarm 的玩法语义）
  - 例：`ActorComponent`、`InventoryComponent`、`CropComponent`、`ResourceNodeComponent`

在实体身上，它们会被“混搭”起来形成实体的最终能力组合：
- 例：玩家实体 = Transform/Sprite/Animation/Collider/Velocity（engine） + Actor/State/Inventory/Hotbar（game）
- 线索：`src/game/factory/entity_factory.cpp`

---

## 4) Tag 的使用方式：marker / dirty / filter / lifecycle
Tag 是“没有数据但有意义”的组件，TinyFarm 里主要有四类用法：

### 4.1 marker：身份/语义标记
- `game::component::PlayerTag`：标记玩家实体
- 作物/资源类型不再依赖 marker tag，而是用组件语义：`CropComponent` 与 `ResourceNodeComponent.type_`（并结合空间层）

### 4.2 dirty：增量更新触发器
用 Tag 表示“需要重新计算/同步”的状态，避免每帧全量扫一遍：
- `engine::component::TransformDirtyTag`：位置/变换变化 → 触发空间索引更新
- `game::component::StateDirtyTag`：状态变化 → 触发动画切换

### 4.3 filter：渲染/逻辑过滤
用 Tag 作为 view 的排除条件（`entt::exclude<...>`）或系统过滤条件：
- `engine::component::InvisibleTag`：渲染/光照系统忽略不可见实体

### 4.4 lifecycle：统一删除（tombstone）
统一用 Tag 表示“要删除”，由专门系统执行删除：
- `engine::component::NeedRemoveTag`：标记实体需要删除
- `engine::system::RemoveEntitySystem`：每帧最早执行，统一销毁实体

好处是：避免“遍地 destroy”，也避免在某个系统遍历 view 时销毁导致的时序/稳定性问题。

对应线索：
- `src/engine/component/tags.h` / `src/game/component/tags.h`
- `src/engine/system/remove_entity_system.*`

---

## 5) System 的基本范式：view 查询 + 顺序更新
System 的典型形态是：
1. 从 registry 创建 view（组件组合 + exclude）
2. 遍历实体，读取组件、写回组件/Tag
3. 必要时通过 dispatcher 发事件解耦跨系统协作

例子：
- Movement：`Velocity + Transform` → 写回 Transform，并打 `TransformDirtyTag`
  - `src/engine/system/movement_system.cpp`
- State：`StateDirtyTag` → enqueue `PlayAnimationEvent`，并清掉 dirty tag
  - `src/game/system/state_system.cpp`

### 更新顺序是“隐式依赖表”
在 TinyFarm 中，系统调度顺序通常写在 Scene 的 `update()` 中（例如 `GameScene::update`）。
顺序背后通常意味着依赖：
- 时间/光照 → 影响渲染与行为
- 移动 → 影响空间索引、相机跟随、交互查询

对应线索：
- `src/game/scene/game_scene.cpp`：系统更新顺序与注释

---

## 6) `registry.ctx()`：场景级“资源/共享数据入口”
`registry.ctx()` 是 EnTT registry 上的“类型到对象”的小容器，适合存放：
- **不属于某个实体**、但属于当前 Scene 的数据
- **很多系统都会读取**、但不想通过单例/全局变量拿的共享数据

TinyFarm 的典型用法：
- `game::data::GameTime`：时间数据放入 ctx，TimeSystem 修改它，其他系统读取它
- `game::world::WorldState*`：世界结构数据指针放入 ctx，供构建/系统读取当前地图信息
- `engine::render::GlobalLightingState`：DayNightSystem 写入 ctx，LightSystem 读取 ctx 并设置渲染器参数

对应线索：
- `src/game/scene/game_scene.cpp`：ctx 初始化（GameTime / WorldState*）
- `src/game/system/time_system.cpp`：读取/更新 GameTime（ctx）
- `src/game/system/day_night_system.cpp`：写入 GlobalLightingState（ctx）
- `src/engine/system/light_system.cpp`：读取 GlobalLightingState（ctx）

---

## 7) 实体如何产生：工厂 / 构建器（数据驱动）
本项目里，实体创建主要来自两条路径：

### 7.1 运行时工厂（EntityFactory）
用于生成“有明确玩法语义”的实体（玩家、动物、作物、掉落物…）：
- `src/game/factory/entity_factory.*`

### 7.2 地图/关卡构建器（EntityBuilder）
用于把关卡数据（Tiled/对象层/属性）映射成组件组合：
- 引擎层：`src/engine/loader/basic_entity_builder.*`
- 游戏层扩展：`src/game/loader/entity_builder.*`

---

## 8) 常见坑：遍历 view 时做结构性修改
在 ECS 中，一个容易踩的坑是：
> **遍历一个 view 的同时，对该 view 相关的 storage 做 remove/destroy 等结构性修改。**

TinyFarm 的建议做法：
- 先把目标实体收集到一个临时数组，再统一处理；或使用 registry 的范围接口（如 `registry.destroy(first, last)`）
- 对 dirty tag（如 `StateDirtyTag`）同理：先收集 dirty entities，再处理并清理 tag

对应线索：
- `src/engine/system/remove_entity_system.cpp`
- `src/game/system/state_system.cpp`

