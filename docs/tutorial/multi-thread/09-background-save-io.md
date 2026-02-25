# 09 - 后台存档 I/O

前面几章介绍了异步加载——把读操作搬到后台。本章反过来看**写操作**：如何在后台保存游戏存档，让玩家感受不到卡顿。

> 核心文件：`src/game/save/save_service.h`、`src/game/save/save_service.cpp`

---

## 问题：同步保存的卡顿

```
saveToFile()
  ├─ 快照当前地图 ECS 状态       (~1ms，registry 遍历)
  ├─ 序列化 JSON                 (~5ms，CPU)
  ├─ 写入磁盘                    (~3ms，IO)
  └─ 总计 ~9ms → 吃掉半帧以上
```

对玩家来说，按下保存键时画面卡一下，体验很差。而且随着游戏进度增长（更多农田、更多NPC），序列化时间只会更长。

**解决方案**：主线程只做快照（~1ms），序列化和写盘搬到后台线程。

---

## 整体架构

```mermaid
sequenceDiagram
    participant P as 玩家
    participant M as 主线程
    participant B as 后台线程

    P->>M: 按下保存键
    M->>M: compare_exchange(false → true)<br/>标记 save_in_progress_

    M->>M: snapshotCurrentMap()<br/>capture() → SaveData

    M->>B: std::jthread([data]{ writeSaveFile() })
    Note over M: 立即返回，继续渲染

    M->>M: 下一帧... 正常更新

    activate B
    B->>B: JSON 序列化
    B->>B: 写入临时文件
    B->>B: 原子 rename
    B-->>M: async_save_result_ = {success, path}
    B->>B: save_in_progress_ = false
    deactivate B

    M->>M: consumeAsyncSaveResult()<br/>检查保存完成
    M->>P: 显示 "已保存 ✓"
```

与异步加载管线（06 章）的关键区别：

| | 异步加载 | 异步保存 |
|---|---|---|
| 线程模型 | 长期运行的 ThreadPool | 每次保存创建一个 jthread |
| 回调机制 | 通过 MainThreadCommandQueue 提交 | 直接写结果到 atomic + mutex |
| 失败策略 | 降级为同步加载 | 返回错误信息，UI 提示 |

为什么不复用线程池？保存是低频操作（几分钟一次），不值得占用长期 worker。`std::jthread` 创建、执行、销毁，干净利落。

---

## 关键数据成员

> `src/game/save/save_service.h`

```cpp
class SaveService final {
    // ... 依赖引用省略 ...

    std::atomic<bool> save_in_progress_{false};           // 防止并发保存
    std::mutex async_result_mutex_{};                     // 保护结果访问
    std::optional<AsyncSaveResult> async_save_result_{};  // 后台写盘结果
    std::optional<std::jthread> async_save_thread_{};     // 后台线程句柄
};
```

```cpp
struct AsyncSaveResult final {
    std::filesystem::path file_path{};
    bool success{false};
    std::string error{};
};
```

三个同步原语各司其职：

```mermaid
flowchart LR
    A["std::atomic﹤bool﹥<br/>save_in_progress_"] --> |"无锁读写<br/>防止并发保存"| X["高频查询路径"]
    B["std::mutex<br/>async_result_mutex_"] --> |"保护写入/读取结果"| Y["低频操作路径"]
    C["std::jthread<br/>async_save_thread_"] --> |"RAII 生命周期管理"| Z["后台线程"]
```

---

## 核心流程：`saveToFileAsync()`

> `src/game/save/save_service.cpp`

```cpp
bool SaveService::saveToFileAsync(const std::filesystem::path& file_path, std::string& out_error) {
    out_error.clear();
    cleanupCompletedSaveThread();

    // 1. 原子 CAS：防止并发保存
    bool expected = false;
    if (!save_in_progress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        out_error = "保存进行中，请稍后再试。";
        return false;
    }

    // 2. 清理上次未消费的结果
    {
        std::lock_guard<std::mutex> lock(async_result_mutex_);
        if (async_save_result_) {
            async_save_result_.reset();
        }
    }

    // 3. 主线程快照（这一步必须在主线程做，因为要访问 registry）
    map_manager_.snapshotCurrentMap();
    SaveData data = capture(out_error);
    if (!out_error.empty()) {
        save_in_progress_.store(false, std::memory_order_release);
        return false;
    }

    // 4. 启动后台线程（data 通过移动语义转移所有权）
    async_save_thread_.emplace([this, data = std::move(data), file_path]() mutable {
        // ═══ 后台线程 ═══
        std::string write_error;
        const bool success = writeSaveFile(data, file_path, write_error);

        AsyncSaveResult result{};
        result.file_path = file_path;
        result.success = success;
        result.error = std::move(write_error);

        {
            std::lock_guard<std::mutex> lock(async_result_mutex_);
            async_save_result_ = std::move(result);
        }

        save_in_progress_.store(false, std::memory_order_release);
    });

    return true;
}
```

### 逐步解析

**步骤 1：`compare_exchange_strong`（CAS）**

```mermaid
flowchart TD
    A["save_in_progress_ 当前值"] --> B{"== expected (false)?"}
    B -->|是| C["原子设为 true ✅<br/>获得保存权"]
    B -->|否| D["保持不变 ❌<br/>说明已有保存在进行"]
```

CAS 是一个原子操作，等价于：
```cpp
// 伪代码（实际是单条原子指令）
if (save_in_progress_ == false) {
    save_in_progress_ = true;
    return true;   // 交换成功
} else {
    expected = save_in_progress_;  // 把当前值写回 expected
    return false;  // 交换失败
}
```

为什么用 CAS 而不是 `if (!flag) flag = true`？因为后者是「读+判断+写」三步操作，两个线程可能同时读到 false，都认为自己抢到了。CAS 是原子的，保证只有一个赢家。

> 在本项目中，保存只在主线程发起，所以并发实际不会发生。但 CAS 是防御性编程，保证即使未来架构变化也不会出 bug。

**步骤 2：清理未消费的旧结果**

```cpp
{
    std::lock_guard<std::mutex> lock(async_result_mutex_);
    if (async_save_result_) {
        async_save_result_.reset();
    }
}
```

正常情况下，调用方每帧都会调用 `consumeAsyncSaveResult()` 取走上次的结果，此处不应有残留。但为了防御性考虑——例如调用方逻辑缺失、或者两次保存间隔极短——这里主动检查并清理，避免旧结果占据 `async_save_result_` 导致新结果写入后逻辑混乱。

注意此操作在 CAS 成功之后执行，所以此时 `save_in_progress_` 已经是 `true`，后台线程不可能正在写入 `async_save_result_`，加锁仅是为了与主线程的 `consumeAsyncSaveResult()` 保持互斥访问的一致性约定。

**步骤 3：主线程快照**

`capture()` 遍历 `entt::registry` 收集所有游戏状态（作物、库存、NPC 等），生成一个自包含的 `SaveData` 值对象。这一步必须在主线程做——因为 registry 不允许并发读写。

**步骤 4：所有权转移**

```cpp
async_save_thread_.emplace([this, data = std::move(data), file_path]() mutable { ... });
```

`std::move(data)` 把数据从主线程转移到后台线程的 lambda 中。转移后主线程不再持有数据，后台线程独占所有权——不需要锁。

这是 05 章提到的**所有权转移模式**在存档场景的应用。

### 无异常约束：读档 JSON 解析

本项目采用无异常风格，`loadFromFile()` 不使用 `try/catch`，改为 `nlohmann::json::parse(..., false)`：

```cpp
nlohmann::json json;
if (!parseJsonFileNoExceptions(file_path, json, out_error)) {
    return false;
}
```

`parseJsonFileNoExceptions()` 的核心检查：

```cpp
out_json = nlohmann::json::parse(content, nullptr, false);
if (out_json.is_discarded()) {
    out_error = "解析存档 JSON 失败: " + file_path.string();
    return false;
}
```

这样解析失败直接走返回值和错误字符串，不依赖异常控制流。

---

## 原子写盘：`writeSaveFile()`

> `src/game/save/save_service.cpp`

```cpp
static bool writeSaveFile(const SaveData& data,
                           const std::filesystem::path& file_path,
                           std::string& out_error) {
    const nlohmann::json json = serialize(data);

    // 1. 确保目录存在
    std::filesystem::create_directories(file_path.parent_path());

    // 2. 写入临时文件
    auto tmp_path = file_path;
    tmp_path += ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        out << json.dump(2);
        out.flush();
        if (!out.good()) {
            out_error = "写入失败: " + tmp_path.string();
            return false;
        }
    }

    // 3. 原子重命名
    std::filesystem::rename(tmp_path, file_path);
    return true;
}
```

**为什么先写临时文件再 rename？**

如果直接写目标文件，写到一半断电，存档就损坏了。`rename` 在大多数文件系统上是原子操作——要么完全完成，要么完全没做。这保证存档文件在任何时刻都是完整的。

```mermaid
flowchart LR
    A["slot0.json<br/>（旧存档，完整）"] --> B["写入 slot0.json.tmp"]
    B --> C["rename(tmp → json)"]
    C --> D["slot0.json<br/>（新存档，完整）"]

    style A fill:#e8f5e9
    style D fill:#e8f5e9
    style B fill:#fff3e0
```

这个模式叫 **write-then-rename**，在数据库、日志系统中广泛使用。

另外注意 `writeSaveFile` 是 `static` 函数——不访问任何成员变量，可以安全地在任何线程调用。

---

## 结果消费：`consumeAsyncSaveResult()`

```cpp
std::optional<AsyncSaveResult> SaveService::consumeAsyncSaveResult() {
    // 快速路径：还在保存中，直接返回
    if (save_in_progress_.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    std::optional<AsyncSaveResult> result;
    {
        std::lock_guard<std::mutex> lock(async_result_mutex_);
        if (!async_save_result_) {
            return std::nullopt;
        }
        result = std::move(async_save_result_);
        async_save_result_.reset();
    }

    cleanupCompletedSaveThread();
    return result;
}
```

**设计细节**：

1. **先检查 atomic 再加锁** —— `save_in_progress_` 的原子读是零成本的，避免每帧都加锁。只有当保存真正完成时才进入 mutex 临界区。

2. **消费即清理** —— 结果被取走后立即 `reset()`，保证不会重复处理。这是一个 **单次消费（consume-once）** 模式。

3. **线程清理** —— `cleanupCompletedSaveThread()` 会 join 已完成的 jthread 并释放资源。

---

## UI 集成模式

保存按钮的典型实现：

```cpp
// 每帧 update
void PauseMenuScene::update(float dt) {
    // 1. 检查异步保存结果
    auto result = save_service_.consumeAsyncSaveResult();
    if (result) {
        if (result->success) {
            showMessage("已保存 ✓");
        } else {
            showMessage("保存失败: " + result->error);
        }
    }
}

// 按钮点击
void PauseMenuScene::onSaveClicked() {
    if (save_service_.isSaving()) {
        return;  // 正在保存，忽略
    }

    std::string error;
    if (!save_service_.saveToFileAsync(slot_path, error)) {
        showMessage("保存失败: " + error);
    }
}
```

```mermaid
stateDiagram-v2
    [*] --> 空闲

    空闲 --> 保存中 : 玩家按下保存<br/>saveToFileAsync()
    保存中 --> 空闲 : consumeAsyncSaveResult()<br/>成功

    保存中 --> 空闲 : consumeAsyncSaveResult()<br/>失败

    note right of 空闲 : 保存按钮可点击
    note right of 保存中 : 保存按钮禁用<br/>显示 "保存中..."
```

**要点**：UI 不用回调、不用 future——每帧轮询一次 `consumeAsyncSaveResult()`，逻辑简单且线程安全。

---

## 与 06 章异步管线的对比

| 维度 | 异步加载管线 (06) | 异步存档 (09) |
|------|-------------------|---------------|
| 数据流方向 | 磁盘 → 内存 → GPU | 内存 → 磁盘 |
| 线程生命周期 | ThreadPool（长期） | jthread（一次性） |
| 主线程交互 | MainThreadCommandQueue | atomic + mutex |
| 并发度 | 多个地图可并行预加载 | 同一时间只允许一次保存 |
| 失败处理 | 降级为同步加载 | 返回错误信息 |
| 结果消费 | 状态机查询 (Ready/Failed) | consumeAsyncSaveResult() |

**共同点**：主线程做最少的工作（快照/创建实体），耗时操作（序列化/解码）交给后台。

---

## 线程安全分析

```
┌──────────────────────────────────────────────────┐
│ save_in_progress_ (atomic)                        │
│                                                    │
│   主线程写：CAS false→true                        │
│   主线程读：isSaving(), consumeAsyncSaveResult()  │
│   后台写：  store(false)                          │
│                                                    │
│   → 无锁，acquire/release 保证可见性             │
├──────────────────────────────────────────────────┤
│ async_save_result_ (mutex 保护)                    │
│                                                    │
│   后台写：  lock + 赋值                           │
│   主线程读：lock + move + reset                   │
│                                                    │
│   → 互斥访问，不会同时读写                       │
├──────────────────────────────────────────────────┤
│ SaveData (所有权转移)                              │
│                                                    │
│   主线程：  capture() 创建 → move 到 lambda       │
│   后台线程：独占使用 → 作用域结束自动析构          │
│                                                    │
│   → 同一时刻只有一个线程拥有数据                 │
└──────────────────────────────────────────────────┘
```

三种机制配合，覆盖了三种不同的数据共享场景：
- **Atomic**：简单标志位，高频读取
- **Mutex**：复杂结构，低频访问
- **所有权转移**：大块数据，零共享

---

## 本章要点

| 概念 | 说明 |
|------|------|
| CAS（compare_exchange） | 原子地抢占保存权，防止并发保存 |
| 所有权转移 | `std::move` 把 SaveData 从主线程转移给后台线程，避免共享 |
| write-then-rename | 先写临时文件再原子重命名，防止断电导致存档损坏 |
| 单次消费模式 | `consumeAsyncSaveResult()` 取走结果后立即清理，不会重复处理 |
| 轮询式 UI | 每帧检查结果，比回调更简单、更安全 |

## 下一篇

[10 - ECS 并行调度](10-ecs-parallel-scheduling.md)：如何把 ECS 系统拆分成可并行执行的任务组，安全地在多线程中更新游戏世界。
