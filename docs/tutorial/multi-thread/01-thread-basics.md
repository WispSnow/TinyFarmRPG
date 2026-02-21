# 01 - C++ 线程基础

## 为什么需要多线程？

TinyFarmRPG 的主循环是严格串行的：

```
输入采样 → fixed tick → frame update → render → 事件结算
```

> 参见 `src/engine/core/game_app.cpp:157`

当 `MapManager::loadMap()` 在主线程同步执行时，文件读取、JSON 解析、图片解码这些耗时操作会直接阻塞渲染——玩家看到的就是「卡了一下」。

多线程的目标：**把耗时的 CPU/IO 工作搬到后台线程，主线程保持流畅渲染**。

---

## `std::thread` vs `std::jthread`

### `std::thread`（C++11）

```cpp
#include <thread>

void worker() {
    // 做一些工作...
}

int main() {
    std::thread t(worker);
    t.join();  // 必须 join 或 detach，否则析构时 std::terminate
}
```

`std::thread` 的问题：
1. **必须手动 `join()` 或 `detach()`**。忘了就崩溃。
2. **没有内建取消机制**。想让线程停下来，需要自己用 `atomic<bool>` 标志。
3. **异常不安全**。如果 `join()` 前抛了异常，线程对象析构时会调用 `std::terminate`。

### `std::jthread`（C++20）

```cpp
#include <thread>

void worker(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        // 做一些工作...
    }
}

int main() {
    std::jthread t(worker);
    // 析构时自动 request_stop() + join()，不需要手动管理
}
```

`std::jthread` 的改进：
1. **RAII 语义**：析构时自动请求停止并等待线程结束。
2. **内建 `std::stop_token`**：线程函数可以通过它检查「是否该退出了」。
3. **异常安全**：无论正常退出还是异常退出，线程都会被正确清理。

> **本项目选择了 `std::jthread`。** 参见 `src/engine/async/thread_pool.h` 中的 `std::vector<std::jthread> workers_`。

---

## `std::stop_token` 协作式取消

`std::stop_token` 是 C++20 引入的**协作式取消**机制。「协作式」意味着：

- 请求方调用 `jthread.request_stop()`，设置取消标志。
- 工作方通过 `stop_token.stop_requested()` **主动检查**标志并决定是否退出。
- 没有人会被强制杀死——线程自己选择何时、如何退出。

### 在线程池中的应用

```cpp
// src/engine/async/thread_pool.cpp — workerLoop

void ThreadPool::workerLoop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        // 阻塞等待任务，同时监听 stop_token
        auto task = queue_.pop(stop_token);
        if (!task) {
            break;  // 队列关闭或收到停止信号
        }
        active_tasks_.fetch_add(1, std::memory_order_relaxed);
        (*task)();
        active_tasks_.fetch_sub(1, std::memory_order_relaxed);
        // ... 通知空闲
    }
}
```

关键点：
- `queue_.pop(stop_token)` 是一个阻塞调用，但它会在 `stop_token` 被触发时**立即醒来**并返回空值。
- 这意味着 worker 不会永远卡在等待任务的状态——当线程池要关闭时，所有 worker 都能及时退出。

### `stop_token` 与条件变量的配合

`std::condition_variable_any`（注意不是 `std::condition_variable`）可以直接接受 `stop_token`：

```cpp
// src/engine/async/work_queue.h — pop 方法

std::optional<T> pop(std::stop_token stop_token) {
    std::unique_lock lock(mutex_);

    // 等待条件：队列非空 或 关闭 或 stop_requested
    bool signaled = cv_not_empty_.wait(lock, stop_token, [this] {
        return !queue_.empty() || closed_;
    });

    if (!signaled || queue_.empty()) {
        return std::nullopt;  // 被取消或队列已关闭且为空
    }
    // ... 取出元素
}
```

> `std::condition_variable_any::wait(lock, stop_token, predicate)` 是 C++20 新增的重载。当 `stop_token` 被触发时，即使谓词不满足也会返回 `false`。这省去了手动检查取消标志的样板代码。

---

## 线程的生命周期管理

在游戏引擎中，线程的启动和关闭顺序至关重要。本项目的做法：

### 启动顺序

```
GameApp::init()
  ├─ initMainThreadCommandQueue()   // 先建好命令队列
  ├─ initContext()                   // 让 Context 持有队列引用
  └─ ...

MapManager::applyLoadingSettings()
  └─ 创建 preload_thread_pool_      // 需要时才建线程池
```

> 参见 `src/engine/core/game_app.cpp:138` 和 `src/game/world/map_manager.cpp`

### 关闭顺序

```
GameApp::close()
  ├─ 场景析构（MapManager 析构）
  │   └─ preload_thread_pool_->stop()  // 先停线程池
  ├─ 等待所有后台任务结束
  └─ 清理资源（GL 上下文、窗口等）
```

**关键原则：先停线程池，再清理资源。** 如果先释放了资源（比如 GL 上下文），而后台线程还在运行并试图访问这些资源，就会产生悬空引用——轻则崩溃，重则数据损坏。

---

## `std::jthread` 的 RAII 优势

对比手动管理：

```cpp
// 手动管理（容易出错）
class BadThreadPool {
    std::vector<std::thread> workers_;
    std::atomic<bool> stopping_{false};

    ~BadThreadPool() {
        stopping_ = true;
        // 如果忘了 join，就 terminate 了
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }
};

// RAII 管理（本项目的做法）
class ThreadPool {
    std::vector<std::jthread> workers_;

    void stop() {
        stopping_ = true;
        queue_.close();
        for (auto& w : workers_) {
            w.request_stop();  // 设置 stop_token
        }
        workers_.clear();      // jthread 析构自动 join
    }
};
```

> 参见 `src/engine/async/thread_pool.cpp` 中的 `stop()` 方法。

`workers_.clear()` 触发每个 `jthread` 的析构函数，析构函数内部会先 `request_stop()` 再 `join()`。这保证了：

1. 所有 worker 都收到停止信号。
2. 所有 worker 都运行完毕后才继续。
3. 即使 `stop()` 过程中抛出异常，其他 `jthread` 的析构仍然会正确执行。

---

## 实验练习

### 练习 1：观察线程 ID

在 `workerLoop` 的开头加一行日志：

```cpp
spdlog::info("worker started on thread {}",
             std::hash<std::thread::id>{}(std::this_thread::get_id()));
```

运行游戏，观察输出。你会发现每个 worker 打印不同的线程 ID，而主循环的日志是另一个 ID。

### 练习 2：感受 `stop_token` 的协作性

在 worker 中加一个人为延迟：

```cpp
void ThreadPool::workerLoop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        auto task = queue_.pop(stop_token);
        if (!task) break;

        spdlog::info("executing task...");
        std::this_thread::sleep_for(std::chrono::seconds(2));  // 模拟耗时任务
        (*task)();
        spdlog::info("task done");
    }
    spdlog::info("worker exiting gracefully");
}
```

然后在游戏启动 1 秒后关闭。你会看到正在执行的任务会**完成当前工作**后才退出——`stop_token` 只是设置了一个标志，不会中断正在执行的代码。

---

## 本章要点

| 概念 | 说明 |
|------|------|
| `std::jthread` | C++20 线程，RAII 管理生命周期，自动 join |
| `std::stop_token` | 协作式取消机制，线程自主检查是否该退出 |
| `condition_variable_any` | 支持 `stop_token` 的条件变量 |
| 启动/关闭顺序 | 先建队列再起线程，先停线程再清资源 |
| RAII | 通过析构函数自动管理资源，异常安全 |

## 下一篇

[02 - 同步原语](02-synchronization-primitives.md)：深入理解 `mutex`、`condition_variable` 和 `atomic`，这些是构建线程安全数据结构的基石。
