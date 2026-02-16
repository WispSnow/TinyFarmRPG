# 从入口到第一帧：main → game::run → GameApp

> 用途：给协作者/读者一个“可复现的启动心智模型”，用于快速定位入口、主循环、初始场景注入点（不逐行讲实现）。

## 启动链路（时序）
```mermaid
sequenceDiagram
  participant Main as main()
  participant Entry as game::run()
  participant App as GameApp
  participant Ctx as Context
  participant Input as InputManager
  participant SM as SceneManager
  participant Scene as TitleScene

  Main->>Entry: call game::run()
  Entry->>App: registerSceneSetup(setupInitialScene)
  Entry->>App: run()
  App->>App: init() (Config/SDL/Renderer/...)
  App->>Ctx: create(...)
  App->>SM: SceneManager(context)
  App->>Entry: scene_setup_func_(context)\n(emit PushSceneEvent)
  App->>App: enter loop
  App->>Input: handleEvents()\nSDL_PollEvent → events
  App->>SM: update(deltaTime)\nprocessPendingActions()
  SM->>Scene: init() (first time)
  App->>SM: render()
  SM->>Scene: render() (first frame)
```

## 每帧节拍（数据流）
```mermaid
flowchart TD
  A["Frame Start"] --> B["Time::update → deltaTime"]
  B --> C["InputManager::update<br/>SDL_PollEvent"]
  C --> D["SceneManager::update<br/>update current scene + apply pending actions"]
  D --> E["Render::clearScreen"]
  E --> F["SceneManager::render<br/>render all scenes"]
  F --> G["Render::present"]
  G --> H["Dispatcher::update<br/>deliver queued events"]
  H --> A
```

## 关键设计点：初始场景由游戏层注入
引擎层（`GameApp`）只负责把“一个可运行的壳”搭起来：初始化子系统、进入主循环、提供渲染/输入/事件分发等基础能力。

“启动时进入哪个 Scene”属于游戏逻辑决策，放在游戏层更合理：
- 游戏层通过 `GameApp::registerSceneSetup(...)` 注入一个回调；
- 回调拿到 `Context` 后，通过 dispatcher 触发 `PushSceneEvent/ReplaceSceneEvent` 把 Scene 交给 `SceneManager` 管理；
- 这样引擎层不需要知道 `TitleScene/GameScene/...`，也更便于以后替换启动流程（例如直接进调试场景、跳过标题、从存档启动等）。

> 事件分发的约定（`trigger`/`enqueue`/`dispatcher.update`）见：`docs/events.md`

