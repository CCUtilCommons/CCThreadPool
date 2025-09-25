# CCThreadPool
CCThreadPool is a simple Thread Pool as a middlewares for other framework, aiming to provide a performance and interface stable library
`CCThreadPool` 是一个现代 C++ 线程池实现，提供高性能、可扩展的线程管理和任务调度接口。它支持：

* 可配置的线程初始数、最小数和最大数。
* 异步任务提交，返回 `std::future`。
* 动态调整线程池大小。
* 安全的任务队列和线程同步。
* 易于包装和扩展接口。

该线程池适用于 CPU 密集型和中等粒度任务的吞吐性能测试，已通过大量单元测试验证（包括压力测试、边界测试、接口异常测试等）。

---

## 2. 功能特性

| 功能      | 描述                                                                                     |
| ------- | -------------------------------------------------------------------------------------- |
| 异步任务提交  | 通过 `enTask()` 提交任意可调用对象，返回 `std::future` 以获取结果。                                        |
| 线程池动态调整 | `resize_thread_count()` 支持动态扩容或缩容线程数量，并保证不超过最大/最小限制。                                   |
| 线程数配置   | 支持 `ThreadCountAccessibleProvider` 接口自定义初始、最大、最小线程数；默认提供 `ThreadCountDefaultProvider`。 |
| 安全关闭    | `shutdown_all()` 可安全关闭所有线程并清空任务队列。                                                     |
| 高性能     | 对微任务采用批量调度策略，减少线程调度和锁竞争开销。                                                             |

---

## 3. 接口说明

### 3.1 线程池构造

```cpp
CCThreadPool::CCThreadPool(
    std::unique_ptr<ThreadCountAccessibleProvider> provider = 
        std::make_unique<ThreadCountDefaultProvider>()
);
```

* `provider`：提供线程数配置，可自定义线程初始数、最大数和最小数。
* 默认提供 `ThreadCountDefaultProvider`。

### 3.2 提交任务

```cpp
template <class Functor, class... Args>
auto enTask(Functor&& functor, Args&&... args) 
    -> std::future<std::invoke_result_t<Functor, Args...>>;
```

* 接收任意可调用对象和参数。
* 返回 `std::future` 用于获取异步结果。
* 内部使用 `std::packaged_task` 封装任务，并放入线程安全队列。

### 3.3 调整线程池大小

```cpp
void resize_thread_count(unsigned int new_count);
void set_thread_max_count(unsigned int cnt);
void set_thread_min_count(unsigned int cnt);
```

* 动态调整线程数量。
* 超过最大或小于最小值会抛出对应异常 (`ThreadCountOverflow` / `ThreadCountUnderflow`)。

### 3.4 关闭线程池

```cpp
void shutdown_all();
```

* 安全关闭线程池。
* 阻塞当前线程直到所有工作线程退出。

---

## 4. 使用示例

```cpp
#include "CCThreadPool.h"
#include <iostream>

int main() {
    CCThreadPool::CCThreadPool pool;

    // 提交任务
    auto fut = pool.enTask([](int a, int b) { return a + b; }, 2, 3);
    std::cout << "Result: " << fut.get() << std::endl;

    // 调整线程池大小
    pool.resize_thread_count(8);

    // 安全关闭
    pool.shutdown_all();
}
```

---

## 5. 性能测试结果（参考）

需要注意的是，这个是笔者运行**AI**生成的Perf test得到的结果，酌情参考
> 测试在单台现代 CPU 上运行 200k 微任务、100k 短任务、20k 中任务、2k 长任务。

| Scenario | Task Time | Threads=1 | Threads=2 | Threads=16 |
| -------- | --------- | --------- | --------- | ---------- |
| Tiny     | 10 µs     | 90k TPS   | 84k TPS   | 83k TPS    |
| Short    | 100 µs    | 6.6k TPS  | 4.3k TPS  | 3.9k TPS   |
| Medium   | 2 ms      | 455 TPS   | 443 TPS   | 438 TPS    |
| Long     | 20 ms     | 49 TPS    | 49 TPS    | 49 TPS     |

* Tiny scenario 批量调度减少线程调度开销。
* Medium/Long scenario CPU-bound，线程数增加效果有限。

---

## 6. 注意事项

1. 对极短任务（<100 µs）建议使用批量提交，避免线程调度和锁竞争占用主要 CPU 时间。
2. `enTask()` 提交任务后，如果线程池已关闭，会抛出 `ThreadPoolTerminateError`。
3. 不可复制或赋值线程池对象（`CCThreadPool` 禁用拷贝构造和赋值操作）。
4. `shutdown_all()` 后不可再提交任务。

---

## 7. 异常处理

| 异常类型                       | 场景          |
| -------------------------- | ----------- |
| `ThreadPoolTerminateError` | 提交任务时线程池已关闭 |
| `ThreadCountOverflow`      | 调整线程数超过最大值  |
| `ThreadCountUnderflow`     | 调整线程数低于最小值  |

---

## 8. 设计理念

* **高性能调度**：批量微任务减少锁和原子操作开销。
* **灵活可配置**：通过 `ThreadCountAccessibleProvider` 可以自定义线程数策略。
* **现代 C++20**：使用 `std::future`、`std::packaged_task` 和模板推导，接口简洁、安全。
* **线程安全**：任务队列、线程管理均由互斥锁和条件变量保护。

---