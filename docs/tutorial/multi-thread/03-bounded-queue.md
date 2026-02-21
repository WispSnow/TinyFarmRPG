# 03 - 有界队列

有界队列（Bounded Queue）是生产者-消费者模式的核心数据结构。本项目的 `WorkQueue<T>` 是线程池的基础——理解它就理解了整个异步系统的任务调度机制。

---

## 为什么要「有界」？

无界队列的风险：如果生产者提交速度远快于消费者处理速度，队列会无限增长，最终耗尽内存。

在游戏场景中：玩家快速切图时，主线程可能连续发起多个预加载请求。如果队列不限容量，worker 来不及处理的任务会堆积，内存占用飙升。

有界队列提供**背压（backpressure）**：队列满时拒绝新任务或让生产者等待，从而控制系统负载。

```
生产者                有界队列 (capacity=4)              消费者
  │                  ┌───┬───┬───┬───┐                   │
  ├─ push(A) ──────► │ A │   │   │   │                   │
  ├─ push(B) ──────► │ A │ B │   │   │                   │
  ├─ push(C) ──────► │ A │ B │ C │   │                   │
  ├─ push(D) ──────► │ A │ B │ C │ D │ ← 满了            │
  ├─ push(E) ──────► │ 拒绝！返回 false │                │
  │                  │ A │ B │ C │ D │ ──────► pop() → A  │
  │                  │   │ B │ C │ D │                    │
  ├─ push(E) ──────► │ E │ B │ C │ D │ ← 现在有空间了    │
```

---

## `WorkQueue<T>` 完整解析

> 源文件：`src/engine/async/work_queue.h`

### 数据成员

```cpp
template<typename T>
class WorkQueue {
    std::size_t capacity_;                      // 最大容量，构造后不变
    mutable std::mutex mutex_;                  // 保护所有状态
    std::deque<T> queue_;                       // FIFO 缓冲区
    bool closed_{false};                        // 关闭标志（不可逆）
    std::condition_variable_any cv_not_empty_;  // 通知：队列有数据了
    std::condition_variable_any cv_not_full_;   // 通知：队列有空间了
};
```

为什么用 `std::deque` 而非 `std::queue`？`deque` 支持随机访问和两端操作，且 `std::queue` 默认就是 `deque` 的适配器。直接用 `deque` 少一层包装。

### 操作 1：非阻塞 push

```cpp
bool push(T value) {
    std::lock_guard lock(mutex_);
    if (closed_ || queue_.size() >= capacity_) {
        return false;  // 已关闭或已满，立即返回
    }
    queue_.push_back(std::move(value));
    cv_not_empty_.notify_one();  // 唤醒一个等待的消费者
    return true;
}
```

**设计选择**：非阻塞版本适用于「任务不重要，丢了也行」的场景。线程池的 `submit()` 在无超时时调用此方法——如果队列满了，任务提交失败，调用者可以选择降级为同步执行。

### 操作 2：带超时的阻塞 push

```cpp
bool pushWithWait(T value, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);

    // 等待空间，最多等 timeout 毫秒
    bool ok = cv_not_full_.wait_for(lock, timeout, [this] {
        return queue_.size() < capacity_ || closed_;
    });

    if (!ok || closed_ || queue_.size() >= capacity_) {
        return false;
    }

    queue_.push_back(std::move(value));
    cv_not_empty_.notify_one();
    return true;
}
```

**为什么要 `unique_lock`**？`condition_variable::wait_for` 需要能临时释放锁（让消费者取走元素腾出空间），所以不能用只支持构造/析构的 `lock_guard`。

**超时的意义**：在游戏中，生产者（主线程）不能永远等待。如果 worker 卡住了，主线程必须在超时后放弃并降级到同步加载，保证渲染不中断。

### 操作 3：阻塞 pop（带取消支持）

```cpp
std::optional<T> pop(std::stop_token stop_token) {
    std::unique_lock lock(mutex_);

    // 等待三个条件之一：有数据 / 队列关闭 / 线程被取消
    bool signaled = cv_not_empty_.wait(lock, stop_token, [this] {
        return !queue_.empty() || closed_;
    });

    // signaled == false 表示 stop_token 被触发
    if (!signaled || queue_.empty()) {
        return std::nullopt;
    }

    T value = std::move(queue_.front());
    queue_.pop_front();
    cv_not_full_.notify_one();  // 唤醒等待空间的生产者
    return value;
}
```

**三路退出**：这是消费者端最精巧的设计。`pop` 可以因为三种原因返回：

1. **队列有数据** → 返回 `optional<T>` 包含值。正常工作路径。
2. **队列关闭且为空** → 返回 `nullopt`。优雅关闭。
3. **`stop_token` 触发** → 返回 `nullopt`。线程被取消。

> 使用 `std::optional` 作为返回类型，优雅地区分了「有值」和「无值」，无需异常或输出参数。

### 操作 4：关闭

```cpp
void close() {
    std::lock_guard lock(mutex_);
    closed_ = true;
    cv_not_empty_.notify_all();  // 唤醒所有消费者
    cv_not_full_.notify_all();   // 唤醒所有生产者
}
```

`close()` 是**不可逆的**——一旦关闭，`push` 始终返回 false，`pop` 在队列清空后返回 `nullopt`。

**为什么 `notify_all` 而不是 `notify_one`？** 关闭时我们需要唤醒**所有**等待线程，让它们检查 `closed_` 标志并退出。`notify_one` 只唤醒一个，其余线程会继续死等。

---

## 状态机视角

```
                 push()
    ┌──────────────────────────────┐
    │                              ▼
 ┌──┴───┐    pop()    ┌────────┐    push()    ┌──────┐
 │ 空   │ ◄────────── │ 有数据 │ ◄──────────  │ 满   │
 │      │             │        │              │      │
 └──────┘             └────────┘              └──┬───┘
    ▲                     │                      │
    │                     │                      │ push() → false
    │                     ▼                      │ 或 pushWithWait()
    │              close() → 所有等待者醒来       │
    └─────────────────────────────────────────────┘
```

队列在三个状态间切换：空（消费者等待）、有数据（正常工作）、满（生产者等待或被拒绝）。`close()` 打断所有等待。

---

## 与 `std::queue` 的对比

```cpp
// 标准库 queue：不是线程安全的，没有容量限制
std::queue<Task> q;
q.push(task);        // 无界，永远成功
auto t = q.front();  // 如果空，未定义行为
q.pop();             // 与 front() 分离，不是原子的

// 本项目 WorkQueue：线程安全，有界，支持取消
WorkQueue<Task> q(256);
q.push(task);           // 满了返回 false
auto t = q.pop(token);  // 空了阻塞等待，支持取消
```

标准库没有提供线程安全的并发队列（截至 C++23），所以这类队列需要自己实现。

---

## 测试验证

> 参见 `tests/engine/async/thread_pool_test.cpp`

### 背压测试

```cpp
TEST(ThreadPoolTest, QueueCapacityAppliesBackpressure) {
    // 1. 创建 capacity=1 的线程池
    ThreadPool pool({.worker_count = 1, .queue_capacity = 1});

    // 2. 提交一个会阻塞的任务，占住 worker
    std::promise<void> blocker;
    pool.submit([&] { blocker.get_future().wait(); });

    // 3. 再提交一个，填满队列（queue_capacity=1）
    pool.submit([] {});  // 应该成功

    // 4. 第三个应该被拒绝
    EXPECT_FALSE(pool.submit([] {}));  // 队列满了

    blocker.set_value();  // 释放阻塞任务
}
```

这个测试验证了背压机制：队列满时新任务被拒绝，调用者可以据此做降级处理。

---

## 容量调优

本项目中的容量配置：

| 组件 | 默认容量 | 配置位置 |
|------|----------|----------|
| `ThreadPool` 工作队列 | 256 | `thread_pool.h` Options |
| `MainThreadCommandQueue` | 4096 | `main_thread_command_queue.h` 默认值 |
| 地图预加载队列 | 32 | `map_loading_settings.h` `async_queue_capacity` |

容量的选择取决于：
- **太小**：频繁触发背压，降级为同步加载的概率增大。
- **太大**：内存占用增加，且大量排队任务可能在关闭时需要等待执行完。
- **经验法则**：预计峰值提交速率 × 平均处理时间 × 2 作为起点。

---

## 本章要点

| 概念 | 说明 |
|------|------|
| 有界队列 | 固定容量，满时拒绝或等待，防止内存无限增长 |
| 背压 | 生产速度超过消费速度时，系统自动减速 |
| 双条件变量 | `cv_not_empty_`（消费者等待）+ `cv_not_full_`（生产者等待） |
| 三路 pop | 有数据返回值 / 关闭返回空 / 取消返回空 |
| `close()` | 不可逆关闭，唤醒所有等待者 |

## 下一篇

[04 - 线程池](04-thread-pool.md)：在有界队列之上构建线程池，管理一组 worker 线程执行异步任务。
