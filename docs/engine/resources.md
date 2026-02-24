# Resource 系统约定：ResourceManager / resource_mapping.json / ID 策略

> 用途：解释 TinyFarm 的资源系统职责边界、`resource_mapping.json` 的数据格式，以及 key-id / path-hash 两种 ID 策略与推荐用法。

## 1) ResourceManager 的职责边界
`engine::resource::ResourceManager` 是一个外观（Facade）：
- **负责**：统一管理 Texture/Sound/Music/Font/UI presets 的加载与缓存；提供调试面板可视化（`Resource`）。
- **不负责**：具体播放逻辑（由 `AudioPlayer` 负责）、具体渲染（由 `Renderer`/各 Pass 负责）、以及“什么时候加载/卸载”的游戏策略（由 GameApp/Scene/System 决定）。

## 2) `assets/data/resource_mapping.json`
这是“语义 key → 文件路径”的映射文件，用于把代码里稳定的 key（例如 `"title-bg-music"`）绑定到真实资源路径：
- `sound`：对象，`{ "<key>": "<path>" }`
- `music`：对象，`{ "<key>": "<path>" }`
- `texture`：对象，`{ "<key>": "<path>" }`
- `font`：对象；每个条目可用 `{ "path": "...", "size": 16 }`（也兼容少量别名/数组写法，便于扩展）
- `ui_button_presets` / `ui_image_presets`：字符串或字符串数组，指向 UI 预设 json

项目启动时 `GameApp` 会调用 `ResourceManager::loadResources("assets/data/resource_mapping.json")`，按上述映射进行预加载。

## 3) 两种 ID 策略（key-id vs path-hash）
### 3.1 key-id（语义 key）
特点：
- 代码里只保存一个**稳定的语义 key**（如 `"title-bg"`），不硬编码真实路径。
- key 本身不包含路径信息，因此“只给 ID”时无法知道去哪加载。

在 TinyFarm 中的约束：
- key-id 之所以能工作，是因为启动阶段读取 `resource_mapping.json` 并进行了预加载。
- 建议在使用处定义 `constexpr entt::hashed_string` 常量（如 `TitleScene` 中的 `TITLE_BG_ID`），避免散落裸字符串并降低拼写错误风险。

### 3.2 path-hash（文件路径哈希）
特点：
- 直接用 **file_path** 生成哈希（ID），并且 `entt::hashed_string.data()` 里携带原始路径。
- 因为路径就携带在调用参数里，所以可以“按需加载”（未命中缓存时用该路径加载）。

注意事项：
- **改路径就会改 ID**（同一个资源会被当成新资源）。
- 更适合：调试资源、临时资源、或不想写入 mapping 的资源。

## 4) 调试与排查建议
打开 DebugUI 的 `Resource` 面板可以查看：
- 每类资源缓存条目数
- 单项资源的来源路径、尺寸/时长、估算内存
- 汇总后的资源预算（帮助建立“缓存不是免费的”的直觉）
