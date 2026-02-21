# 项目设计文档: TinyFarm (教学演示项目)

## 项目简介

TinyFarm 是一款受经典游戏《星露谷物语》启发的2D农场经营模拟游戏的技术演示项目。本项目的核心目的并非创造一个完整的商业游戏，而是以教学为导向，系统性地展示如何使用现代C++技术栈和实体组件系统（ECS）架构来构建一个功能模块化、数据驱动、易于扩展的游戏。

## 技术栈

- **构建系统**: CMake 3.13+
- **窗口与输入**: SDL3
- **ECS 框架**: EnTT
- **图形渲染**: OpenGL + GLAD
- **UI 框架**: ImGui
- **资源加载**: stb_image.h
- **字体渲染**: FreeType + HarfBuzz
- **音频**: MiniAudio
- **数学库**: GLM
- **日志**: spdlog
- **JSON 解析**: nlohmann-json
- **脚本宿主（可选）**: Lua 5.4.8 + Sol2 v3.5.0（project version 4.0.0）
- **测试框架**: Google Test
- **地图编辑**: Tiled (生成 .tmj 格式地图)

## 目录结构

> 仅列出目录级结构与模块职责。具体文件请使用 Glob/Grep 工具查询。

```
TinyFarmRPG/
├── src/
│   ├── engine/                  # 可复用游戏引擎层
│   │   ├── async/               #   多线程基础设施（WorkQueue/ThreadPool/MainThreadCommandQueue）
│   │   ├── audio/               #   音频播放（MiniAudio）
│   │   ├── component/           #   引擎层 ECS 组件（transform/sprite/collider/light 等）
│   │   ├── core/                #   应用生命周期、全局上下文 Context、配置、游戏状态（含主线程命令提交点）
│   │   ├── debug/               #   ImGui 调试面板框架与内置面板
│   │   ├── input/               #   输入映射与动作事件
│   │   ├── loader/              #   Tiled 地图/关卡加载器（含 LevelPreprocessService 异步预处理）
│   │   ├── render/              #   OpenGL 多通道渲染管线（场景/光照/泛光/合成/UI）
│   │   ├── resource/            #   纹理/音频/字体统一资源管理（含 ImageDecode/FontPreprocess）
│   │   ├── scene/               #   场景基类 Scene 与场景管理器 SceneManager
│   │   ├── spatial/             #   碰撞检测与空间分区（静态网格/动态网格）
│   │   ├── system/              #   引擎层 ECS 系统（动画/移动/渲染/Y排序/光照）
│   │   ├── ui/                  #   UI 框架（布局/状态机/行为/通用组件）
│   │   └── utils/               #   工具函数（数学/对齐/事件定义）
│   ├── game/                    # 游戏特定逻辑层
│   │   ├── component/           #   游戏组件（作物/库存/快捷栏/NPC/地图/宝箱/拾取等）
│   │   ├── data/                #   游戏数据（GameTime / ItemCatalog）
│   │   ├── debug/               #   游戏层 ImGui 调试面板
│   │   ├── defs/                #   Command / Event / 常量 / 作物 / 音频ID 定义
│   │   ├── domain/              #   领域服务（InventoryDomainService 统一写入入口）
│   │   ├── factory/             #   实体蓝图 Blueprint 与工厂 EntityFactory
│   │   ├── loader/              #   游戏实体构建器（Tiled 约定扩展）
│   │   ├── runtime/             #   运行时装配 GameRuntimeAssembler 与系统调度 SystemScheduler
│   │   ├── save/                #   存档系统（序列化/schema 迁移/槽位管理）
│   │   ├── scene/               #   游戏场景（Title/GameScene/PauseMenu/SaveSlotSelect/RestDialog）
│   │   ├── script/              #   Lua 脚本宿主层（可选，ScriptHost/bindings/句柄校验）
│   │   ├── system/              #   游戏 ECS 系统（农场/交互/NPC/对话/地图切换/物品使用等）
│   │   ├── ui/                  #   游戏 UI（物品栏/快捷栏/对话气泡/时钟/tooltip）
│   │   └── world/               #   世界地图系统（MapManager 异步预加载状态机/WorldState/快照序列化）
│   └── main.cpp                 # 可执行入口薄壳
├── assets/                      # 运行时资源
│   ├── audio/                   #   音频文件 (.wav)
│   ├── data/                    #   JSON 配置（蓝图/作物/物品/对话/光照/地图加载策略等）
│   ├── fonts/                   #   字体文件 (.ttf)
│   ├── maps/                    #   Tiled 地图 (.tmj/.tsj)
│   ├── shaders/                 #   GLSL 着色器 (.vert/.frag)
│   └── textures/                #   纹理资源 (.png/.gif/.json)
├── scripts/                     # Lua 脚本（可选，ENABLE_SCRIPTING=ON 时部署）
├── config/                      # 引擎配置（窗口/输入/渲染/音频/文本）
├── cmake/                       # CMake 构建模块（依赖管理/编译器设置/ImGui集成等）
├── external/                    # 第三方库源码（Lua/Sol2 等）
├── tests/                       # Google Test 单元测试（含 async 队列预算测试、MapManager 异步预加载测试）
├── plans/                       # 开发计划文档（Phase 1/2 多线程改造计划）
├── docs/                        # 项目文档（含 tutorial/multi-thread 渐进式并发改造说明）
└── for_agent/                   # AI Agent 编码规范
```
