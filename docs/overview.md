# 项目设计文档: TinyFarm (教学演示项目)

## 项目简介

TinyFarm 是一款受经典游戏《星露谷物语》启发的2D农场经营模拟游戏的技术演示项目。本项目的核心目的并非创造一个完整的商业游戏，而是以教学为导向，系统性地展示如何使用现代C++技术栈和实体组件系统（ECS）架构来构建一个功能模块化、数据驱动、易于扩展的游戏。

## 技术栈

- **构建系统**: CMake 3.10+
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
- **测试框架**: Google Test
- **地图编辑**: Tiled (生成 .tmj 格式地图)

## 目录结构

```
game_engine/
├── src/
│   ├── engine/                      # 游戏引擎核心代码（可复用）
│   │   ├── audio/                   # 音频播放器
│   │   │   ├── audio_player.cpp/h   # 音频播放器实现   
│   │   │   ├── stb_vorbis_impl.c    # 用于实现 std_vorbis
│   │   ├── component/               # 引擎层 ECS 组件定义
│   │   │   ├── animation_component.h    # 动画组件
│   │   │   ├── audio_component.h        # 音频组件
│   │   │   ├── auto_tile_component.h    # 自动瓦片组件
│   │   │   ├── collider_component.h     # 碰撞盒组件
│   │   │   ├── name_component.h         # 实体名称组件
│   │   │   ├── parallax_component.h     # 视差滚动组件
│   │   │   ├── render_component.h       # 渲染排序组件
│   │   │   ├── sprite_component.h       # 精灵组件
│   │   │   ├── tilelayer_component.h    # 瓦片层组件
│   │   │   ├── transform_component.h    # 变换组件
│   │   │   ├── velocity_component.h     # 速度组件
│   │   │   ├── light_component.h        # 点光/聚光/自发光组件
│   │   │   ├── tags.h                   # 标签组件
│   │   ├── core/                    # 核心系统
│   │   │   ├── game_app.cpp/h       # 主应用程序类
│   │   │   ├── context.cpp/h        # 全局上下文
│   │   │   ├── time.cpp/h           # 时间管理
│   │   │   ├── config.cpp/h         # 配置管理
│   │   │   ├── game_state.cpp/h     # 游戏状态管理
│   │   ├── debug/                   # 调试面板系统
│   │   │   ├── debug_panel.h               # 调试面板基类
│   │   │   ├── debug_ui_manager.cpp/h      # 调试UI管理器
│   │   │   ├── dispatcher_trace.cpp/h      # 事件分发追踪
│   │   │   └── panels/                     # 调试面板
│   │   │       ├── audio_debug_panel.cpp/h         # 音频调试面板
│   │   │       ├── game_state_debug_panel.cpp/h    # 游戏状态调试面板
│   │   │       ├── gl_renderer_debug_panel.cpp/h   # 渲染器调试面板
│   │   │       ├── input_debug_panel.cpp/h         # 输入调试面板
│   │   │       ├── res_mgr_debug_panel.cpp/h       # 资源管理调试面板
│   │   │       ├── scene_debug_panel.cpp/h         # 场景调试面板
│   │   │       ├── spatial_index_debug_panel.cpp/h # 空间索引调试面板
│   │   │       ├── text_renderer_debug_panel.cpp/h # 文本渲染调试面板
│   │   │       ├── time_debug_panel.cpp/h          # 时间调试面板
│   │   │       ├── ui_preset_debug_panel.cpp/h     # UI预设调试面板
│   │   │       └── dispatcher_trace_debug_panel.cpp/h # 事件分发追踪调试面板
│   │   ├── input/                   # 输入管理
│   │   │   └── input_manager.cpp/h  # 输入管理器
│   │   ├── loader/                  # 关卡/地图加载器
│   │   │   ├── basic_entity_builder.cpp/h # 基础实体构建器
│   │   │   ├── level_loader.cpp/h   # 关卡加载器
│   │   │   ├── tiled_conventions.h  # Tiled 约定定义
│   │   │   ├── tiled_diagnostics.h  # Tiled 诊断工具
│   │   │   ├── tiled_json_cache.h   # Tiled JSON 缓存
│   │   │   └── tiled_json_helpers.h # Tiled JSON 辅助函数
│   │   ├── render/                  # 渲染系统
│   │   │   ├── lighting_state.h     # 全局光照参数
│   │   │   ├── camera.cpp/h         # 相机系统
│   │   │   ├── image.cpp/h          # 图像渲染
│   │   │   ├── nine_slice.cpp/h     # 九宫格渲染
│   │   │   ├── renderer.cpp/h       # 渲染器接口
│   │   │   ├── text_renderer.cpp/h  # 文本渲染器
│   │   │   └── opengl/              # OpenGL 渲染实现
│   │   │       ├── bloom_pass.cpp/h     # 泛光渲染通道
│   │   │       ├── composite_pass.cpp/h # 合成渲染通道
│   │   │       ├── emissive_pass.cpp/h  # 自发光渲染通道
│   │   │       ├── gl_helper.h         # OpenGL 辅助函数
│   │   │       ├── gl_renderer.cpp/h    # OpenGL渲染器
│   │   │       ├── imgui_layer.cpp/h    # ImGui 渲染层
│   │   │       ├── lighting_pass.cpp/h  # 光照渲染通道
│   │   │       ├── render_context.cpp/h # 渲染上下文
│   │   │       ├── renderer_init_params.h # 渲染器初始化参数
│   │   │       ├── scene_pass.cpp/h     # 场景渲染通道
│   │   │       ├── shader_asset_paths.h # 着色器资源路径
│   │   │       ├── shader_library.cpp/h # 着色器库
│   │   │       ├── shader_program.cpp/h # 着色器程序
│   │   │       ├── sprite_batch.cpp/h   # 精灵批处理
│   │   │       ├── ui_pass.cpp/h        # UI渲染通道
│   │   │       └── viewport_manager.cpp/h # 视口管理器
│   │   ├── resource/                # 资源管理
│   │   │   ├── audio_manager.cpp/h      # 音频资源管理
│   │   │   ├── auto_tile_library.cpp/h  # 自动瓦片库
│   │   │   ├── font_manager.cpp/h       # 字体管理
│   │   │   ├── resource_debug_info.h    # 资源调试信息
│   │   │   ├── resource_manager.cpp/h   # 统一资源管理器
│   │   │   └── texture_manager.cpp/h    # 纹理管理
│   │   ├── scene/                   # 场景管理系统
│   │   │   ├── scene.cpp/h          # 场景基类
│   │   │   └── scene_manager.cpp/h  # 场景管理器
│   │   ├── spatial/                 # 空间分区及索引
│   │   │   ├── collision_resolver.cpp/h    # 碰撞解决器
│   │   │   ├── dynamic_entity_grid.cpp/h   # 动态实体网格
│   │   │   ├── spatial_index_manager.cpp/h # 空间索引管理器
│   │   │   └── static_tile_grid.cpp/h      # 静态瓦片网格
│   │   ├── system/                  # 引擎层 ECS 系统
│   │   │   ├── fwd.h                      # 系统前置声明
│   │   │   ├── animation_system.cpp/h      # 动画系统
│   │   │   ├── audio_system.cpp/h          # 音频系统
│   │   │   ├── auto_tile_system.cpp/h      # 自动瓦片系统
│   │   │   ├── debug_render_system.cpp/h   # 调试渲染系统
│   │   │   ├── movement_system.cpp/h       # 移动系统
│   │   │   ├── remove_entity_system.cpp/h  # 实体移除系统
│   │   │   ├── render_system.cpp/h         # 渲染系统
│   │   │   ├── spatial_index_system.cpp/h  # 空间索引系统
│   │   │   ├── light_system.cpp/h          # 光照系统
│   │   │   └── ysort_system.cpp/h          # Y轴排序系统
│   │   ├── ui/                      # UI 系统
│   │   │   ├── behavior/                # UI行为
│   │   │   │   ├── drag_behavior.h       # 拖拽行为
│   │   │   │   ├── hover_behavior.h      # 悬浮行为
│   │   │   │   └── interaction_behavior.h # 交互行为
│   │   │   ├── layout/                  # UI布局
│   │   │   │   ├── ui_grid_layout.cpp/h      # 网格布局
│   │   │   │   ├── ui_layout.cpp/h           # 基础布局
│   │   │   │   └── ui_stack_layout.cpp/h     # 堆栈布局
│   │   │   ├── state/                   # UI状态
│   │   │   │   ├── ui_state.h               # UI状态基类/接口
│   │   │   │   ├── ui_hover_state.cpp/h     # 悬停状态
│   │   │   │   ├── ui_normal_state.cpp/h    # 正常状态
│   │   │   │   └── ui_pressed_state.cpp/h   # 按下状态
│   │   │   ├── ui_button.cpp            # 按钮组件
│   │   │   ├── ui_defaults.h            # UI默认设置
│   │   │   ├── ui_drag_preview.cpp      # 拖拽预览
│   │   │   ├── ui_draggable_panel.cpp/h # 可拖拽面板
│   │   │   ├── ui_element.cpp           # UI元素基类
│   │   │   ├── ui_image.cpp             # 图片组件
│   │   │   ├── ui_input_blocker.cpp     # 用于屏蔽鼠标输入的透明层
│   │   │   ├── ui_interactive.h         # 交互式UI
│   │   │   ├── ui_item_slot.cpp         # 物品槽位组件
│   │   │   ├── ui_label.h               # 标签组件
│   │   │   ├── ui_manager.cpp           # UI管理器
│   │   │   ├── ui_panel.h               # 面板组件
│   │   │   ├── ui_preset_manager.h      # UI预设管理器
│   │   │   ├── ui_progress_bar.cpp      # 进度条组件
│   │   │   ├── ui_progress_bar.cpp/h   # 进度条组件
│   │   │   └── ui_screen_fade.h         # 全屏淡入淡出遮罩
│   │   └── utils/                   # 工具函数
│   │       ├── alignment.h           # 对齐工具
│   │       ├── defs.h                # 通用定义
│   │       ├── events.h              # 事件定义
│   │       ├── scoped_timer.h        # RAII计时工具
│   │       └── math.h                # 数学工具
│   ├── game/                        # 游戏特定代码
│   │   ├── component/               # 游戏特定组件
│   │   │   ├── action_sound_component.h # 行为音效组件
│   │   │   ├── actor_component.h     # 角色组件
│   │   │   ├── npc_component.h       # NPC/动物组件（漫游、睡眠/进食、对话）
│   │   │   ├── crop_component.h      # 作物组件
│   │   │   ├── farmland_component.h  # 耕地组件
│   │   │   ├── hotbar_component.h    # 快捷栏组件
│   │   │   ├── inventory_component.h # 物品栏组件
│   │   │   ├── state_component.h     # 状态组件
│   │   │   ├── tags.h                # 游戏标签
│   │   │   ├── target_component.h    # 目标组件
│   │   │   ├── map_component.h       # 地图组件（MapId、MapTrigger、RestArea）
│   │   │   ├── chest_component.h     # 宝箱组件（奖励物品）
│   │   │   ├── pickup_component.h    # 拾取组件（掉落物品）
│   │   │   └── resource_node_component.h # 资源节点组件（树木、石头等可采集资源）
│   │   ├── data/                    # 游戏数据
│   │   │   ├── game_time.cpp/h       # 游戏时间数据
│   │   │   └── item_catalog.cpp/h    # 物品目录
│   │   ├── debug/                   # 游戏调试面板
│   │   │   ├── blueprint_inspector_debug_panel.cpp/h # 蓝图检查调试面板
│   │   │   │   ├── game_time_debug_panel.cpp/h     # 游戏时间调试面板
│   │   │   ├── inventory_debug_panel.cpp/h     # 物品栏调试面板
│   │   │   ├── map_inspector_debug_panel.cpp/h # 地图检查调试面板
│   │   │   ├── map_loading_debug_panel.cpp/h   # 地图载入设置调试面板
│   │   │   ├── player_debug_panel.cpp/h        # 玩家调试面板
│   │   │   └── save_load_debug_panel.cpp/h     # 存档/读档调试面板
│   │   ├── defs/                    # 游戏预定义
│   │   │   │   ├── audio_ids.h           # 音频ID定义
│   │   │   │   ├── constants.h           # 常量定义
│   │   │   ├── crop_defs.h           # 作物定义
│   │   │   │   ├── render_layers.h       # 渲染层定义
│   │   │   │   └── spatial_layers.h      # 空间层定义
│   │   │   │   └── events.h              # 游戏事件定义
│   │   ├── factory/                 # 实体工厂和蓝图
│   │   │   ├── blueprint.h           # 实体蓝图
│   │   │   ├── blueprint_manager.cpp/h # 蓝图管理器
│   │   │   └── entity_factory.cpp/h  # 实体工厂
│   │   ├── loader/                  # 游戏特定加载器
│   │   │   ├── entity_builder.cpp/h  # 游戏实体构建器
│   │   │   └── tiled_conventions.h  # Tiled 约定定义
│   │   ├── world/                   # 世界地图系统
│   │   │   ├── world_state.cpp/h           # 世界状态管理（地图元数据、可变状态）
│   │   │   ├── map_loading_settings.cpp/h  # 读取并保存地图加载策略
│   │   │   ├── map_manager.cpp/h           # 地图管理器（加载/卸载、状态快照）
│   │   │   └── map_snapshot_serializer.cpp/h # 地图快照序列化工具（离线生长补偿）
│   │   ├── save/                    # 存档系统
│   │   │   ├── save_data.cpp/h      # 存档数据结构（JSON序列化/反序列化）
│   │   │   ├── save_data.cpp/h      # 存档数据结构（JSON序列化/反序列化）
│   │   │   ├── save_service.cpp/h   # 存档服务（统一save/load接口）
│   │   │   └── save_slot_summary.cpp/h # 存档槽位摘要
│   │   ├── scene/                   # 游戏场景
│   │   │   ├── save_slot_select_scene.cpp/h    # 存档/读档场景
│   │   │   ├── pause_menu_scene.cpp/h          # 菜单场景
│   │   │   ├── rest_dialog_scene.cpp/h         # 休息对话框场景（选择休息时长并快进时间）
│   │   │   ├── title_scene.cpp/h               # 标题场景
│   │   │   └── game_scene.cpp/h                # 主游戏场景
│   │   ├── system/                  # 游戏层 ECS 系统
│   │   │   ├── fwd.h                         # 系统前置声明
│   │   │   ├── action_sound_system.cpp/h     # 行为音效系统
│   │   │   ├── animation_event_system.cpp/h    # 动画事件系统
│   │   │   ├── camera_follow_system.cpp/h     # 相机跟随系统
│   │   │   ├── chest_system.cpp/h             # 宝箱系统
│   │   │   ├── crop_system.cpp/h              # 作物生长系统
│   │   │   ├── day_night_system.cpp/h         # 昼夜循环系统
│   │   │   ├── time_of_day_light_system.cpp/h  # 切换"昼夜限定光源(day_only/night_only)"的启用状态
│   │   │   ├── farm_system.cpp/h              # 农场操作系统
│   │   │   ├── hotbar_system.cpp/h            # 快捷栏系统
│   │   │   ├── interaction_system.cpp/h       # 交互系统
│   │   │   ├── inventory_system.cpp/h         # 物品栏系统
│   │   │   ├── item_use_system.cpp/h          # 物品使用系统
│   │   │   ├── player_control_system.cpp/h    # 玩家控制系统
│   │   │   ├── render_target_system.cpp/h     # 渲染目标系统
│   │   │   ├── state_system.cpp/h             # 状态管理系统
│   │   │   ├── time_system.cpp/h              # 时间更新系统（含休息/睡觉时间快进辅助函数）
│   │   │   ├── npc_wander_system.cpp/h        # NPC 随机游走
│   │   │   ├── animal_behavior_system.cpp/h   # 动物睡眠/进食
│   │   │   ├── dialogue_system.cpp/h          # 对话触发与事件分发
│   │   │   ├── rest_system.cpp/h              # 休息交互系统（打开休息对话框）
│   │   │   ├── map_transition_system.cpp/h    # 地图切换系统
│   │   │   ├── pickup_system.cpp/h            # 拾取系统（物品拾取逻辑）
│   │   │   ├── light_toggle_system.cpp/h     # 光照切换系统
│   │   │   └── system_helpers.h              # 系统辅助函数
│   │   ├── ui/                      # 游戏UI
│   │   │   ├── dialogue_bubble.cpp/h  # 对话气泡 UI（事件驱动）
│   │   │   ├── item_tooltip_ui.cpp/h  # 鼠标悬浮时显示 UI
│   │   │   ├── hotbar_ui.cpp/h       # 快捷栏UI
│   │   │   ├── inventory_ui.cpp/h    # 物品栏UI
│   │   │   ├── time_clock_ui.cpp/h   # 时钟UI
│   │   │   └── ui_drag_drop_helpers.h # UI拖拽辅助函数
│   │   └── game_entry.cpp/h         # 程序入口
│   └── main.cpp                     # exe文件薄壳
├── assets/                          # 游戏资源
│   ├── audio/                       # 音频文件 (.wav)
│   ├── data/                        # 数据文件 (JSON配置、蓝图等)
│   │   ├── actor_blueprint.json     # 角色/NPC 蓝图（player/friend，动画、对话、移动）
│   │   ├── animal_blueprint.json    # 动物蓝图（sheep/cow，wander/sleep/eat动画）
│   │   ├── crop_config.json         # 农作物配置（strawberry/potato生长阶段）
│   │   ├── dialogue_script.json     # 对话文本表（NPC对话内容）
│   │   ├── dialogue_script_readme.md # 对话脚本说明文档
│   │   ├── game_time_config.json    # 游戏时间系统配置（昼夜周期、时间流逝）
│   │   ├── icon_config.json         # 图标配置（tools/crops/seeds/materials/indicator/time）
│   │   ├── item_config.json         # 物品配置（工具、作物、种子、材料属性）
│   │   ├── light_config.json        # 昼夜光照系统配置（太阳/月亮/环境光参数）
│   │   ├── light_config_readme.md   # 光照配置说明文档
│   │   ├── map_loading_config.json  # 地图预加载策略设置（`off/neighbors/all`）
│   │   ├── resource_mapping.json    # 资源映射文件（UI预设、音效路径）
│   │   ├── ui_button_presets.json   # UI按钮预设配置（primary/secondary/page按钮样式）
│   │   └── ui_image_presets.json    # UI图像预设配置（inventory/quickbar/dialogue bubble样式）
│   ├── fonts/                       # 字体文件 (.ttf)
│   ├── maps/                        # 地图文件 (.tmj, .tsj)
│   ├── shaders/                     # 着色器文件 (.vert, .frag)
│   ├── textures/                    # 纹理资源 (.png, .gif, .json)
│   └── farm-rpg/                    # Farm RPG 素材资源包 (2863+ 文件)
├── config/                          # 配置文件
│   ├── audio.json                   # 音频配置
│   ├── input.json                   # 输入配置
│   ├── render.json                  # 渲染配置
│   ├── text_render.json             # 文本渲染配置
│   └── window.json                  # 窗口和快捷栏配置
├── cmake/                           # CMake构建配置
│   ├── BuildHelpers.cmake           # 构建辅助函数
│   ├── CompilerSettings.cmake       # 编译器设置
│   ├── Dependencies.cmake           # 依赖管理
│   ├── ImGui.cmake                  # ImGui集成
│   ├── OpenGL.cmake                 # OpenGL配置
│   │   ├── ProjectInfo.cmake        # 项目信息
│   │   ├── RuntimePath.cmake        # 运行时路径
│   │   └── scripts/                 # 构建脚本
│   └── QUICK_REFERENCE.md           # CMake快速参考
├── external/                        # 第三方库
├── tests/                           # 单元测试
├── tools/                           # 开发工具
└── docs/                        # 参考文档
```
