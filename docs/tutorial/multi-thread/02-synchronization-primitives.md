# 02 - 同步原语

当多个线程访问共享数据时，如果不加保护，就会产生**数据竞争（data race）**——一种未定义行为。本章介绍 C++ 提供的三类同步原语，以及它们在本项目中的使用。

---

## 1. 互斥量 `std::mutex`

互斥量是最基本的同步工具：同一时刻只有一个线程能持有锁。

### 基本用法

```cpp
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);  // 构造时加锁，析构时解锁
    ++shared_counter;  // 临界区：只有持锁线程能执行
}
```

### `lock_guard` vs `unique_lock`

| 特性 | `lock_guard` | `unique_lock` |
|------|-------------|---------------|
| 灵活性 | 构造时锁，析构时解锁，不可中途操作 | 可手动 lock/unlock、延迟锁定、转移所有权 |
| 性能 | 零开销 | 略微多一个状态标志 |
| 配合 `condition_variable` | 不可以 | 可以（CV 需要在等待时临时解锁） |
| 使用场景 | 简单的临界区保护 | 需要条件变量或更精细控制时 |

### 在本项目中的使用

`MainThreadCommandQueue` 使用 `std::mutex` 保护命令队列：

```cpp
// src/engine/async/main_thread_command_queue.h

class MainThreadCommandQueue {
    mutable std::mutex mutex_;
    std::deque<Command> queue_;

    bool enqueue(Command command) {
        std::lock_guard lock(mutex_);       // 简单保护，用 lock_guard
        if (queue_.size() >= capacity_) return false;
        queue_.push_back(std::move(command));
        return true;
    }

    bool enqueueWithWait(Command command, int timeout_ms) {
        std::unique_lock lock(mutex_);      // 需要配合 CV，用 unique_lock
        // cv_not_full_.wait_for(lock, ...);
        // ...
    }
};
```

> **`mutable`**：即使通过 `const` 引用访问对象，也能对 `mutex_` 加锁。这在 `size()` 等 const 查询方法中是必要的。

---

## 2. 条件变量 `std::condition_variable`

互斥量解决了「互斥访问」，但有时线程需要**等待某个条件成立**。轮询（busy wait）会浪费 CPU，条件变量提供了高效的等待/通知机制。

### 核心操作

```
wait(lock, predicate)    — 释放锁，休眠，直到被通知且 predicate 为 true
notify_one()             — 唤醒一个等待线程
notify_all()             — 唤醒所有等待线程
```

### 为什么需要 predicate（谓词）？

```cpp
// 错误：裸 wait，可能虚假唤醒
cv.wait(lock);

// 正确：带谓词的 wait
cv.wait(lock, [this] { return !queue_.empty(); });
```

条件变量存在**虚假唤醒（spurious wakeup）**——线程可能在没有人调用 `notify` 的情况下醒来。带谓词的 `wait` 内部会在每次醒来时检查条件，如果不满足就继续睡。

```mermaid
sequenceDiagram
    participant P as 生产者
    participant CV as condition_variable
    participant C as 消费者

    Note over C: cv.wait(lock) 等待

    Note over CV: 系统虚假唤醒（无人 notify）
    CV-->>C: 醒来 ⚡
    Note over C: ❌ 裸 wait：直接继续<br/>queue 可能仍为空！

    rect rgb(240, 255, 240)
        Note over C: cv.wait(lock, predicate) 等待
        CV-->>C: 醒来 ⚡（虚假）
        C->>C: 检查 !queue_.empty() → false
        C->>CV: 继续休眠
        P->>CV: push() + notify_one()
        CV-->>C: 醒来 ✅（真实）
        C->>C: 检查 !queue_.empty() → true
        Note over C: 安全取出并处理
    end
```

`wait(lock, predicate)` 等价于：
```cpp
while (!predicate()) {
    cv.wait(lock);  // 虚假醒来后 predicate 为 false，继续等
}
```

### 在本项目中的应用：双条件变量模式

`WorkQueue` 使用两个条件变量管理有界队列：

```cpp
// src/engine/async/work_queue.h

template<typename T>
class WorkQueue {
    std::mutex mutex_;
    std::deque<T> queue_;
    std::size_t capacity_;
    bool closed_{false};

    std::condition_variable_any cv_not_empty_;  // 消费者等待：「队列非空」
    std::condition_variable_any cv_not_full_;   // 生产者等待：「队列未满」
};
```

**生产者（提交任务）**：

```cpp
bool pushWithWait(T value, int timeout_ms) {
    std::unique_lock lock(mutex_);

    // 等待队列有空间，最多等 timeout_ms 毫秒
    bool has_space = cv_not_full_.wait_for(lock,
        std::chrono::milliseconds(timeout_ms),
        [this] { return queue_.size() < capacity_ || closed_; });

    if (!has_space || closed_) return false;

    queue_.push_back(std::move(value));
    cv_not_empty_.notify_one();  // 通知消费者：有新任务了
    return true;
}
```

**消费者（取出任务）**：

```cpp
std::optional<T> pop(std::stop_token stop_token) {
    std::unique_lock lock(mutex_);

    // 等待队列非空 或 关闭 或 stop_requested
    bool signaled = cv_not_empty_.wait(lock, stop_token, [this] {
        return !queue_.empty() || closed_;
    });

    if (!signaled || queue_.empty()) return std::nullopt;

    T value = std::move(queue_.front());
    queue_.pop_front();
    cv_not_full_.notify_one();  // 通知生产者：有空间了
    return value;
}
```

**信号流图**：

```mermaid
sequenceDiagram
    participant P as 生产者（主线程）
    participant Q as WorkQueue
    participant C as 消费者（Worker）

    Note over C: pop() 阻塞等待<br/>cv_not_empty_

    P->>Q: push(task)
    Q->>C: cv_not_empty_.notify_one()
    activate C
    C->>Q: 取出任务
    C->>Q: cv_not_full_.notify_one()
    deactivate C

    Note over Q: 队列已满

    P->>Q: pushWithWait(task)
    Note over P: 等待 cv_not_full_...
    C->>Q: pop() 取出一个元素
    Q->>P: cv_not_full_.notify_one()
    Note over P: 醒来，放入任务
```

### `condition_variable` vs `condition_variable_any`

| 特性 | `condition_variable` | `condition_variable_any` |
|------|---------------------|-------------------------|
| 锁类型 | 只能用 `std::unique_lock<std::mutex>` | 任意满足 BasicLockable 的锁 |
| `stop_token` 支持 | 不支持 | 支持（C++20） |
| 性能 | 可能更快（更少间接层） | 略有开销 |
| 本项目选择 | — | 选用此类，因为需要 `stop_token` 支持 |

### `stop_token`：线程优雅退出

`std::jthread`（C++20）会自动向工作线程传递 `stop_token`，当外部调用 `jthread::request_stop()` 时，`stop_token::stop_requested()` 变为 `true`。

`condition_variable_any` 的特殊重载能直接感知 `stop_token`，**无需轮询**：

```cpp
// WorkQueue::pop 中（简化版）
std::optional<T> pop(std::stop_token stop_token) {
    std::unique_lock lock(mutex_);

    // 满足以下任一条件均会返回：
    //   1. queue_ 非空（predicate 为 true）
    //   2. 外部请求停止（stop_token.stop_requested()）
    bool signaled = cv_not_empty_.wait(lock, stop_token,
        [this] { return !queue_.empty() || closed_; });

    if (!signaled || queue_.empty()) return std::nullopt;  // 收到停止信号
    // ...
}
```

当 `ThreadPool::stop()` 被调用时：
1. 所有 Worker 的 `stop_token` 触发
2. `cv_not_empty_.wait(...)` 立即返回 `false`（`!signaled`）
3. Worker 安全退出，无需额外的 `closed_` 标志通知

```mermaid
sequenceDiagram
    participant Owner as 调用方（析构/stop）
    participant JT as jthread（Worker）
    participant CV as cv_not_empty_

    Owner->>JT: request_stop()
    JT->>CV: stop_token 触发
    Note over CV: wait() 被唤醒
    CV-->>JT: 返回 false（stop 信号）
    JT->>JT: return std::nullopt → 退出循环
    JT-->>Owner: 线程自然退出（jthread 析构时自动 join）
```

---

## 3. 原子操作 `std::atomic`

对于简单的计数器和标志，`atomic` 提供无锁的线程安全操作，比 mutex 高效得多。

### 基本用法

```cpp
#include <atomic>

std::atomic<int> counter{0};

// 线程 A
counter.fetch_add(1, std::memory_order_relaxed);  // 原子递增

// 线程 B
int value = counter.load(std::memory_order_relaxed);  // 原子读取
```

### 内存序（Memory Ordering）

这是 `atomic` 最容易困惑的部分。简化理解：

| 内存序 | 含义 | 性能 | 使用场景 |
|--------|------|------|----------|
| `relaxed` | 只保证原子性，不保证顺序 | 最快 | 纯计数器 |
| `acquire` | 读操作后的代码不会被重排到读之前 | 中等 | 读取「发布」的数据 |
| `release` | 写操作前的代码不会被重排到写之后 | 中等 | 「发布」数据给其他线程 |
| `acq_rel` | acquire + release | 中等 | 读-修改-写操作（如 fetch_add） |
| `seq_cst` | 全局顺序一致（默认值） | 最慢 | 不确定时用这个 |

### 用 relaxed 传递数据会出什么问题？

```cpp
std::atomic<bool> ready{false};
std::string data;  // 普通变量，非 atomic

// 线程 A（写者）
data = "hello";
ready.store(true, std::memory_order_relaxed);  // ❌ 错！

// 线程 B（读者）
while (!ready.load(std::memory_order_relaxed)) {}
std::cout << data;  // 危险：可能看到空字符串！
```

**为什么会出错**：`relaxed` 只保证 `ready` 本身的原子性，不约束与其他内存操作的顺序。编译器或 CPU 可以把 `data = "hello"` 的写入**重排到** `ready.store()` 之后。线程 B 看到 `ready == true` 时，`data` 可能还没被写入。

**正确做法：用 release-acquire 建立 happens-before**：

```cpp
// 线程 A
data = "hello";
ready.store(true, std::memory_order_release);  // ✅ release：之前的写入对读者可见

// 线程 B
while (!ready.load(std::memory_order_acquire)) {}  // ✅ acquire：能看到 release 前的所有写入
std::cout << data;  // 安全
```

**`acq_rel` 的场景**——同时读出旧值并写入新值（读-修改-写）时：

```cpp
// generation 计数器递增：读取旧值、+1、写回新值，这三步是一个原子 RMW
// 需要同时"发布"新值（release）又"获取"当前最新值（acquire）
task.shared->generation.fetch_add(1, std::memory_order_acq_rel);
```

### 在本项目中的使用

**1. 任务活跃计数器**（`ThreadPool`）：

```cpp
// src/engine/async/thread_pool.h
std::atomic<std::size_t> active_tasks_{0};

// workerLoop 中
active_tasks_.fetch_add(1, std::memory_order_relaxed);  // 开始执行
(*task)();
active_tasks_.fetch_sub(1, std::memory_order_relaxed);  // 执行完毕
```

这里用 `relaxed` 是安全的，因为 `active_tasks_` 只用于统计和 `waitForIdle()` 判断——不需要与其他内存操作建立 happens-before 关系。

**2. 停止标志**（`ThreadPool`）：

```cpp
// src/engine/async/thread_pool.h
std::atomic<bool> stopping_{false};

bool submit(Task task) {
    if (stopping_.load(std::memory_order_relaxed)) {
        return false;  // 拒绝新任务
    }
    // ...
}
```

**3. 任务状态与 generation 计数器**（`MapManager`）：

```cpp
// src/game/world/async_preload_pipeline.h
struct AsyncPreloadTaskState {
    std::atomic<MapPreloadTaskState> state{MapPreloadTaskState::NotScheduled};
    std::atomic<std::uint64_t> generation{0};
};
```

`generation` 使用 `acquire/release` 语义：

```cpp
// 写端（主线程调度时）
shared->generation.store(gen, std::memory_order_release);

// 读端（worker 线程检查时）
if (shared->generation.load(std::memory_order_acquire) != expected_gen) {
    return;  // 任务已过期，丢弃
}
```

`release` 保证：调度时设置的所有数据（路径、配置等）在 worker 读到 generation 时都可见。
`acquire` 保证：worker 读到 generation 后，能看到调度时写入的所有数据。

```mermaid
sequenceDiagram
    participant M as 主线程
    participant G as generation (atomic)
    participant W as Worker 线程

    M->>M: 设置路径、配置等数据
    M->>G: store(gen, release) 🔒
    Note over M,G: release 保证：<br/>之前的写入对读者可见

    G->>W: load(acquire) 🔓
    Note over G,W: acquire 保证：<br/>能看到 store 之前的所有写入
    W->>W: 安全读取路径、配置等数据
    Note over M,W: release → acquire 构成 happens-before 关系
```

> 这对 `release-acquire` 构成了一个 **happens-before** 关系，是跨线程数据传递的最低开销方式。

---

## 选择指南：何时用什么？

```mermaid
flowchart TD
    Start{需要保护什么？} --> A{只是计数器或标志？}
    A -->|是| A1["用 std::atomic<br/>（无锁，最轻量）"]
    A -->|否| B{需要等待某个条件成立？}
    B -->|是| B1["用 condition_variable<br/>+ mutex + unique_lock"]
    B -->|否| C{读多写少？}
    C -->|是| C1["用 shared_mutex<br/>（见第 07 章）"]
    C -->|否| D["用 mutex<br/>+ lock_guard / unique_lock"]

    style A1 fill:#e8f5e9
    style B1 fill:#e3f2fd
    style C1 fill:#fff3e0
    style D fill:#fce4ec
```

---

## 常见陷阱

### 1. 死锁

```cpp
// 线程 A：先锁 mutex1，再锁 mutex2
std::lock_guard lock1(mutex1);
std::lock_guard lock2(mutex2);

// 线程 B：先锁 mutex2，再锁 mutex1
std::lock_guard lock2(mutex2);
std::lock_guard lock1(mutex1);
// → 死锁！A 等 B 释放 mutex2，B 等 A 释放 mutex1
```

```mermaid
sequenceDiagram
    participant A as 线程 A
    participant B as 线程 B

    A->>A: lock(mutex1) ✅
    B->>B: lock(mutex2) ✅
    A->>A: lock(mutex2) → 阻塞 🔒
    B->>B: lock(mutex1) → 阻塞 🔒
    Note over A,B: 死锁：双方永远等待
```

**解决方案**：`std::scoped_lock` 内部使用死锁避免算法，一次性原子地锁住多个 mutex：

```cpp
// 无论线程 A 和 B 以何种顺序调用，都不会死锁
void transfer(Account& from, Account& to, int amount) {
    std::scoped_lock lock(from.mutex, to.mutex);  // 自动协商加锁顺序
    from.balance -= amount;
    to.balance += amount;
}
```

> `scoped_lock` 对单个 mutex 也可以用，效果等同于 `lock_guard`。

### 2. 锁范围过大

```cpp
// 不好：持锁时做 IO
{
    std::lock_guard lock(mutex_);
    auto data = readFileFromDisk(path);  // 阻塞 IO！其他线程全部等待
    cache_[key] = data;
}

// 好：只在必要时持锁
auto data = readFileFromDisk(path);  // 无锁
{
    std::lock_guard lock(mutex_);
    cache_[key] = data;  // 只保护写入
}
```

本项目的 `MainThreadCommandQueue::drain()` 就遵循了这个原则——在锁内取出命令，在锁外执行命令（详见第 05 章）。

### 3. 忘记 notify

```cpp
void push(T value) {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(value));
    // 忘了 cv_not_empty_.notify_one(); → 消费者永远等不到通知
}
```

---

## 4. Owner-Thread 模式（无锁设计）

有时最好的锁是**根本不需要锁**。如果能保证某块数据只从一个固定线程访问，就不需要任何同步原语。

本项目的 `AsyncPreloadPipeline` 就采用了这个模式：

```cpp
class AsyncPreloadPipeline {
    std::thread::id owner_thread_id_;  // 构造时记录所有者线程

    bool ensureOwnerThread(std::string_view api_name) const {
        if (std::this_thread::get_id() == owner_thread_id_) {
            return true;
        }
        spdlog::warn("AsyncPreloadPipeline::{} called from non-owner thread", api_name);
        return false;
    }

    // 所有公共 API 在入口处检查
    bool schedule(entt::id_type map_id, std::string_view level_path) {
        if (!ensureOwnerThread("schedule")) return false;
        // ... 安全访问 async_preload_tasks_（无锁！）
    }
};
```

```mermaid
flowchart LR
    subgraph MT["主线程（owner）"]
        A[schedule / clearTasks<br/>getTaskState]
        A -->|无锁访问| B[(async_preload_tasks_\n unordered_map)]
    end

    subgraph WT["Worker 线程"]
        C[lambda]
        C -->|原子写| D[shared->state\nshared->generation]
    end

    B --- |shared_ptr| D
```

**关键分工**：
- **主线程**：独占访问 `async_preload_tasks_`（map 结构），无需加锁
- **Worker**：只通过原子字段（`state`、`generation`）回写状态，不碰主线程的数据结构

**适用条件**：
- 数据有明确的"所有者线程"（通常是主线程/渲染线程）
- Worker 只需要通过共享的原子控制块汇报状态，不需要修改主线程的容器
- 比为每个操作加锁的性能开销更低

> 这个模式在游戏引擎中很常见：ECS 的 registry 通常也只允许主线程读写，Worker 只能通过命令队列提交变更。

---

## 本章要点

| 原语 | 用途 | 本项目使用位置 |
|------|------|---------------|
| `std::mutex` | 保护共享数据的互斥访问 | `MainThreadCommandQueue`、`WorkQueue` |
| `std::condition_variable_any` | 线程间等待/通知，支持 `stop_token` | `WorkQueue` 的 `cv_not_empty_`/`cv_not_full_` |
| `std::atomic` | 无锁的简单数据操作 | `ThreadPool::active_tasks_`、`stopping_`；`AsyncPreloadPipeline::generation` |
| `std::lock_guard` | RAII 互斥锁（简单场景） | `enqueue()`、`size()` |
| `std::unique_lock` | 灵活互斥锁（配合 CV） | `pop()`、`pushWithWait()` |
| `std::scoped_lock` | 同时锁住多个 mutex，防死锁 | 多资源临界区 |
| Owner-Thread 模式 | 约定单线程访问，完全消除锁 | `AsyncPreloadPipeline` 的主 map 结构 |

## 下一篇

[03 - 有界队列](03-bounded-queue.md)：用本章的原语组合出一个线程安全的有界队列——本项目线程基础设施的核心组件。
