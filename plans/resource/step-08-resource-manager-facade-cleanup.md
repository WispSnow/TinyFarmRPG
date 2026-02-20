# 步骤 8: 精简 `ResourceManager` Facade，完成收口

- 对应上层计划：`plans/resource-refactor.md`

## 思路

本步骤基于当前真实代码状态收口，而不是重复做已完成清理：

- 步骤 6/7 已完成：去 path/hash get 重载、strict preload、生效事件时序修复
- 本步骤核心只剩两件实事：
- 去掉子管理器 `friend + private-only`，提升可测试性
- 收敛 Facade/Manager 的命名与编排一致性（尤其 `clear()` 路径、类型命名、职责边界）

最终方案：

- 保留 `ResourceManager` 作为统一编排入口（不拆散到调用方直接依赖多 manager）
- 不再新增/维护任何过渡兼容接口
- 只做高价值收口改造，不重复前序步骤已完成工作

按 `for_agent/design-guide.md`，本步骤不考虑向后兼容，直接按最终形态收敛。

## 关键设计

1. 8.1 从“清理”改为“验证 + 防回归”

- 当前仓库已无 `[[deprecated]]` 过渡方法、无 path/hash get 重载、无 ResourceManager 的 AutoTile/UIPreset 转发。
- 因此 8.1 不再做“删除实现”，改为：
- 固化现状结论（文档 + 编译期断言）
- 防止后续回归重新引入旧入口

2. 去 `friend` 的精确范围

- `TextureManager` / `AudioManager` / `FontManager` 去掉 `friend class ResourceManager`。
- 提升为 `public` 的仅限业务操作接口：
- `load*` / `find*` / `unload*` / `clear*` / `collect*DebugInfo`
- 保持 `FontManager` 的构造与初始化约束不变：
- `create()` 继续是唯一构造入口
- 默认构造函数与 `init()` 继续保持内部使用（不对外开放）

3. `ResourceManager::clear()` 编排路径统一

- 明确统一策略：`clear()` 仅通过 Facade 自身的 `clearFonts/clearSounds/clearMusic/clearTextures` 调度，不直接调用子管理器聚合方法。
- 目的：
- 路径一致，便于后续插入事件/统计钩子
- 避免局部绕过导致行为分叉

4. `loadResources()` 策略明确

- 本步骤不重写 JSON 解析器、不拆新模块；保持现有加载能力。
- 仅做职责声明：`loadResources()` 继续作为 Facade 配置入口存在，解析复杂度优化不在步骤 8 范围。

5. 命名与不对称项收敛（显式列出）

- 统一项：
- manager 的核心操作接口命名保持 load/find/unload/clear 风格
- debug 采集命名保留各资源实际语义，但暴露方式保持一致（可被外部测试调用）
- 已知不对称处理策略：
- `TextureManager::findTexture` 维持非 `const`（受 `TextureHandle` 类型与 EnTT 接口约束）
- `Font` 返回类型保持裸指针，但引入显式类型别名 `FontHandle`（`using FontHandle = Font*`）来统一 API 表达层

6. AutoTile Debug 类型耦合处理

- 当前 `resource_debug_info.h` 包含 `AutoTileRuleDebugInfo/AutoTileTopology`，与步骤 3 的职责分离目标不一致。
- 本步骤将 AutoTile debug 结构迁移到独立头文件（如 `auto_tile_debug_info.h`），从 `resource_debug_info.h` 移除。

## 需要新增的文件

| 文件 | 说明 |
|------|------|
| `src/engine/resource/auto_tile_debug_info.h` | 承载 `AutoTileRuleDebugInfo` 与相关类型，解除对 `resource_debug_info.h` 的耦合 |
| `tests/engine/resource/resource_manager_submanager_visibility_test.cpp` | 编译期校验去 friend 后子管理器关键接口可访问 |
| `tests/engine/resource/resource_manager_clear_orchestration_test.cpp` | 运行时校验 `ResourceManager::clear()` 统一编排路径后行为一致（含字体缓存清理与事件行为） |

> 需要同步更新（无新增）：`src/engine/resource/resource_manager.h`、`src/engine/resource/resource_manager.cpp`、`src/engine/resource/texture_manager.h`、`src/engine/resource/audio_manager.h`、`src/engine/resource/font_manager.h`、`src/engine/resource/resource_debug_info.h`、`src/engine/resource/auto_tile_library.h/.cpp`、`src/engine/debug/panels/res_mgr_debug_panel.*`、`tests/CMakeLists.txt`。
> 复用现有测试：`resource_manager_strict_preload_api_test.cpp`、`resource_manager_texture_handle_api_test.cpp`、`resource_manager_audio_handle_api_test.cpp`（不新增重复断言）。

## 实现步骤

### 8.1 现状验证与防回归基线

- 把“过渡 API 已清零”的事实写入步骤说明与测试职责边界。
- 不做重复清理代码，仅补防回归断言（复用/扩展现有测试而非复制）。

### 8.2 去 `friend` 并开放子管理器业务操作接口

- 删除三个 manager 的 `friend class ResourceManager`。
- 将业务操作接口提升到 `public`。
- 保持 `FontManager::create()` 工厂模式与内部初始化封装不变。

### 8.3 收敛 Facade 编排与命名

- 统一 `ResourceManager::clear()` 调度路径，只走 Facade 的四类 clear 接口。
- 引入 `FontHandle` 类型别名，统一 ResourceManager API 的表达层（底层仍为 `Font*`，不迁移 cache 模型）。
- 明确 `loadResources()` 在步骤 8 不做大规模改造，仅保持行为稳定。

### 8.4 拆分 AutoTile debug 类型耦合

- 新建 `auto_tile_debug_info.h`，迁移 AutoTile debug 相关结构。
- `resource_debug_info.h` 保留 Texture/Audio/Font 相关结构，避免职责混杂。
- 更新引用方（AutoTileLibrary、DebugPanel 等）。

### 8.5 测试与构建接入

- 新增：
- `resource_manager_submanager_visibility_test.cpp`
- `resource_manager_clear_orchestration_test.cpp`
- 更新 `tests/CMakeLists.txt`。
- 构建与测试：
- `cmake --build build -j 8`
- `ctest --test-dir build --output-on-failure`

### 8.6 手动回归

- Title -> Game 切换：无资源回退加载告警。
- 资源清理边界（退出/切场景）：无字体悬垂、无纹理句柄泄漏断言。
- 资源调试面板：Texture/Audio/Font 与 AutoTile 调试信息正常显示。

## 待办

- [x] 8.1 明确“过渡 API 已清零”并完成防回归基线对齐（不做重复清理）
- [x] 8.2 去掉三个子管理器的 `friend` 并公开业务操作接口
- [x] 8.3 统一 `ResourceManager::clear()` 编排路径（仅走 Facade clear 接口）
- [x] 8.3A 引入 `FontHandle` 类型别名并统一 Facade 层接口表达
- [x] 8.4 将 AutoTile debug 类型移出 `resource_debug_info.h`
- [x] 8.5 新增并接入子管理器可见性与 clear 编排测试
- [x] 8.6 完成构建、全量自动化测试与关键场景手动回归

## 需要澄清

暂无。
