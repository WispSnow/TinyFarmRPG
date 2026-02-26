# 调试与崩溃定位指南

## CMake Presets

项目根目录的 `CMakePresets.json` 提供了以下构建预设：

| 预设 | 说明 |
|------|------|
| `debug` | 标准 Debug，保留符号信息，无优化 |
| `debug-asan` | Debug + AddressSanitizer，检测内存错误 |
| `debug-tsan` | Debug + ThreadSanitizer，检测数据竞争 |
| `release` | Release 优化构建 |
| `relwithdebinfo` | Release 优化但保留调试符号 |

```bash
# 配置
cmake --preset <预设名>

# 构建
cmake --build --preset <预设名>

# 可执行文件位置
./build-<预设名>/TinyFarm-Darwin      # macOS
./build-<预设名>/TinyFarm-Linux       # Linux
```

## AddressSanitizer (ASan)

检测内存类错误：堆/栈越界、use-after-free、double-free、内存泄漏。

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan
./build-debug-asan/TinyFarm-Darwin
```

崩溃时 ASan 会自动在终端输出报告，包含文件名和行号：

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
    #0 0x... in MyClass::update() src/game/system/xxx.cpp:42
    #1 0x... in SystemScheduler::tick() src/game/runtime/system_scheduler.cpp:108
```

常用环境变量：

```bash
# 检测内存泄漏（macOS 默认关闭）
ASAN_OPTIONS=detect_leaks=1 ./build-debug-asan/TinyFarm-Darwin

# 崩溃时不立即终止，尽可能多报告错误
ASAN_OPTIONS=halt_on_error=0 ./build-debug-asan/TinyFarm-Darwin
```

> **注意**：ASan 不支持 MSVC，且不能与 TSan 同时使用。

## ThreadSanitizer (TSan)

检测多线程数据竞争，复用项目已有的 `ENABLE_TSAN` 选项。

```bash
cmake --preset debug-tsan && cmake --build --preset debug-tsan
./build-debug-tsan/TinyFarm-Darwin
```

TSan 检测到竞争时输出示例：

```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T2:
    #0 SomeSystem::update() src/game/system/some_system.cpp:30
  Previous read of size 4 at 0x... by main thread:
    #0 Renderer::draw() src/engine/render/renderer.cpp:55
```

> **注意**：TSan 不能与 ASan 同时使用，需要分别构建。

## LLDB 调试器

macOS 自带的调试器，适合交互式定位崩溃。

### 基本用法

```bash
lldb ./build-debug/TinyFarm-Darwin
(lldb) run                    # 启动程序
# 崩溃后自动暂停
(lldb) bt                     # 当前线程调用栈
(lldb) bt all                 # 所有线程调用栈
(lldb) frame variable         # 当前帧局部变量
(lldb) frame select 3         # 切换到第 3 帧
(lldb) print myVar            # 打印变量值
(lldb) quit                   # 退出
```

### 设置断点

```bash
(lldb) b src/game/scene/game_scene.cpp:100    # 文件行号断点
(lldb) b GameScene::update                     # 函数断点
(lldb) run
# 命中断点后
(lldb) next                   # 单步（不进入函数）
(lldb) step                   # 单步（进入函数）
(lldb) continue               # 继续执行
```

### 崩溃时信号捕获

```bash
(lldb) process handle SIGSEGV --stop true      # 段错误时暂停
(lldb) process handle SIGABRT --stop true      # abort 时暂停
```

## 如何收集信息给 AI 定位问题

崩溃时，复制以下信息提供给 AI 工具即可快速定位：

1. **ASan/TSan 报告**（终端自动输出，包含文件名和行号，最有价值）
2. **LLDB `bt` / `bt all` 输出**（完整调用栈）
3. **相关源码片段**（报告中指出的文件和函数）
4. **复现步骤**（触发崩溃的操作序列）

### 示例：提交给 AI 的模板

```
## 崩溃报告

**构建预设**: debug-asan
**复现步骤**: 进入农场地图 → 使用锄头 → 崩溃

**ASan 输出**:
（粘贴完整输出）

**相关源码**:
（粘贴报告中提到的文件关键片段）
```

## 预设选择指南

| 场景 | 推荐预设 |
|------|----------|
| 日常开发调试 | `debug` |
| 怀疑内存错误（野指针/越界） | `debug-asan` |
| 怀疑多线程竞争 | `debug-tsan` |
| 性能测试/基准测试 | `release` |
| 线上问题定位 | `relwithdebinfo` |
