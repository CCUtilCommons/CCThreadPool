# CC手搓项目文档——线程池

> 本文档隶属于项目：[一个简单的C++动态线程池](https://github.com/CCUtilCommons/CCThreadPool.git)，所有的源代码也在这里

## 什么是线程池

​	线程池是一种池化技术，相似的我们还有内存池。都是池。如何理解这一类池对我们的开发非常重要。

​	举一个例子吧。比如说您现在正在给花浇水，但是水龙头离您很远，很不好开关。这个时候您会选择拿一个超级大的容器蓄满水，然后运到花园附近用这个池子的水浇花，而不用反复的跑到水源地附件操作。我们实际上一次性的就把水接好，然后用这些水做事情。

​	这样的资源池化技术的思路是共同的：

- 水一下子不好搞到——资源的一次申请开销较大，但是如果发挥临近性原则，总体开销要小于多次重复的单次开销。从而将运行时间可以拖延到启动时。
- 我们运行时的接口不再直接跟资源本身的创建和销毁对接，所有的资源申请，使用和释放都走资源池。这样资源本身的存在由专门的组件托管。我们压根不用关心资源的申请和释放了

​	对于本篇，就是我们在利用这样的**资源池化技术**解决**线程的频繁创建和销毁所带来显著的性能开销拖缓我们软件运行执行效率的问题**。得到的产物就是线程池（Thread Pool）

​	线程池的定义可以概括为：**一个预先创建并维护了一组工作线程（Worker Threads）的集合。** 这些线程被组织起来，用于执行提交给线程池的任务队列（Task Queue）中的任务。

​	我们可以用一个生活中的例子来理解：假设一个银行有多个服务窗口（工作线程），前来办理业务的客户（任务）络绎不绝。如果没有线程池，就相当于每来一位客户，银行就临时搭建一个新的窗口，客户办完业务后再拆除，这样效率极低。而线程池则相当于银行预先开设了固定数量的窗口，客户来了就排队取号（进入任务队列），哪个窗口空闲了，就叫号请下一位客户上前办理。这样，银行既避免了反复搭建和拆除窗口的成本，又能有效地服务所有客户。

在这个比喻中：

- **银行** 就是我们的应用程序。
- **服务窗口** 就是线程池中的工作线程。
- **客户** 就是需要执行的异步任务。
- **等候区和叫号系统** 就是任务队列。

#### 老生常谈的：它有什么用？

1. **降低资源消耗**：线程的创建和销毁是需要消耗系统资源的。通过复用已经创建的线程，线程池极大地减少了因频繁创建和销毁线程所带来的性能开销，从而降低了系统的整体资源消耗。
2. **提高响应速度**：当有新任务到达时，线程池中通常有处于等待状态的空闲线程，可以立即执行该任务，免去了创建新线程所需的时间。这使得任务能够更快地得到处理，从而提高了应用程序的响应速度。
3. **提高线程的可管理性**：线程是稀缺资源，如果无限制地创建，不仅会消耗大量系统资源，还可能因线程数量超过系统承载能力而导致性能下降甚至系统崩溃。线程池可以对池中的线程进行统一的分配、调优和监控，开发者可以根据系统负载情况，动态地调整线程池的大小，从而有效地控制并发线程的数量，防止资源过度竞争。
4. **提供更强大的功能**：许多现代编程语言中的线程池实现，如Java的 `ExecutorService` 框架，还提供了诸如定时执行、周期性执行任务、并发控制等高级功能，进一步简化了并发编程的复杂性。

## 手搓一个线程池，我们需要做哪些工作？

一个完整的线程池通常具备以下几个核心功能，这些功能共同协作，构成了其高效稳定的工作流程：

- **线程管理**：这是线程池最基本也是最重要的功能。它负责线程的创建、复用和销毁。线程池会根据配置参数（如核心线程数、最大线程数等）来动态调整池中线程的数量，以适应不同的负载情况。
- **任务队列**：线程池内部通常会维护一个任务队列，用于存放等待执行的任务。当所有工作线程都在忙碌时，新提交的任务会被放入这个队列中排队等候。任务队列的类型（如有界队列、无界队列等）也会影响线程池的行为。
- **任务接收与拒绝策略**：线程池需要提供一个接口，用于接收外部提交的任务。而且还要有机制将我们执行的内容返回给调用者

**线程池的典型工作流程如下：**

1. **提交任务**：应用程序创建一个任务（通常是一个实现了特定接口的对象，如Java中的 `Runnable` 或 `Callable`），并将其提交给线程池。
2. **线程池处理**：线程池接收到任务后，会进行判断：
   - 如果当前工作线程数小于核心线程数（Core Pool Size），则创建一个新的工作线程来执行该任务。
   - 如果当前工作线程数已经达到核心线程数，则将任务放入任务队列中。
   - 如果任务队列已满，但当前工作线程数小于最大线程数（Maximum Pool Size），则会创建新的非核心线程来执行该任务。
3. **线程执行任务**：工作线程从任务队列中获取任务并执行。
4. **线程复用**：一个工作线程在完成任务后，并不会立即销毁，而是会回到线程池中，继续等待并获取新的任务来执行。如果一个线程在指定的时间内（Keep-Alive Time）没有接收到新任务，那么（非核心）线程可能会被销毁，以释放资源。

## 行动起来

基于此，这里的线程池的每一个组件的职责非常清晰了——

- `ThreadCountAccessibleProvider`，线程创建的时候，我们对线程池个数的约束
- `CCThreadPool`也就是我们的线程池抽象本体。

### `ThreadCountAccessibleProvider`：线程池的资源约束

```cpp
/**
 * @brief   ThreadCountAccessibleProvider provide how many thread
 *          should we get for the configures count;
 *			To override the behaviors of Provider, rewrite the provide*
 *			API to adjust, see ThreadCountDefaultProvider as an example
 *
 */
struct ThreadCountAccessibleProvider {
	unsigned int getThreadInitCount() const; ///< interative interfaces
	unsigned int getThreadMaxCount() const; ///< interative interfaces
	unsigned int getThreadMinCount() const; ///< interative interfaces
	virtual ~ThreadCountAccessibleProvider() = default;

private:
	virtual unsigned int provideThreadInitCount() const noexcept = 0;
	virtual unsigned int provideThreadMaxCount() const noexcept = 0;
	virtual unsigned int provideThreadMinCount() const noexcept = 0;
}; // ThreadCountAccessibleProvider
```

​	这里当然是一种检查机制了，所有的provide*函数是你需要重写的接口。

```cpp
using namespace CCThreadPool;

/**
 * @brief MIN_ACCEPT_THREAD shows the pass check limits for user provides
 *
 */
static constexpr const unsigned int MIN_ACCEPT_THREAD = 1;
static_assert(MIN_ACCEPT_THREAD >= 1, "Thread Count should be literal ge than 1");

// 这里就是我们的重写范例了，默认的情况下，我们直接提供一半
unsigned int ThreadCountDefaultProvider::provideThreadInitCount() const noexcept {
	return std::thread::hardware_concurrency() / 2;
}

// 最多给多少？
unsigned int ThreadCountDefaultProvider::provideThreadMaxCount() const noexcept {
	return std::thread::hardware_concurrency();
}

// 最少？
unsigned int ThreadCountDefaultProvider::provideThreadMinCount() const noexcept {
	return 1;
}

namespace {
inline bool trigger_check(const int given_cnt,
                          const int min_count,
                          const int max_count) {
	return min_count > given_cnt || given_cnt > max_count;
}

};

// 所有的Get接口托管了检查机制，所以这个时候我们不用自己手动处理
unsigned int ThreadCountAccessibleProvider::getThreadInitCount() const {
	const unsigned int cnt = provideThreadInitCount();
	const unsigned int minCount = getThreadMinCount();
	const unsigned int maxCount = getThreadMaxCount();
	if (trigger_check(cnt, minCount, maxCount)) {
		if (cnt < minCount) {
			throw ThreadCountUnderflow(cnt, minCount, maxCount);
		} else {
			throw ThreadCountOverflow(cnt, minCount, maxCount);
		}
	}
	return cnt;
}

unsigned int ThreadCountAccessibleProvider::getThreadMaxCount() const {
	return provideThreadMaxCount();
}

unsigned int ThreadCountAccessibleProvider::getThreadMinCount() const {
	const unsigned int cnt = provideThreadMinCount();
	if (cnt < MIN_ACCEPT_THREAD) { // 如果少于我们编译时期的指定，也会直接抛出异常
		throw ThreadCountRangeError(cnt, MIN_ACCEPT_THREAD);
	}
	return cnt;
}
```

## CCThreadPool接口设计

​	在行动之前，我们务必要搞明白哪一些接口是我们要做的。这里笔者就不想重复思路了。您对比我在开头说的内容，就能知道我们到底要做哪些工作了。

| 函数签名                                                     | 说明                                                         |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| `CCThreadPool(const std::unique_ptr<ThreadCountAccessibleProvider> provider = ...)` | **构造函数**。创建 `CCThreadPool` 实例。接受一个可选参数 `provider`，用于提供线程计数的访问能力，默认为 `ThreadCountDefaultProvider` 的实例。 |
| `virtual ~CCThreadPool()`                                    | **析构函数**。销毁 `CCThreadPool` 实例，并在销毁前调用 `shutdown_all()` 来停止所有工作线程。 |
| `template <class Funtor, class... RequestArguments> auto enTask(Funtor&& functor, RequestArguments&&... requestArgs) -> std::future<FutureWrapType<Funtor, RequestArguments...>>` | **提交任务**。将一个可调用对象（**`functor`**）及其参数（**`requestArgs`**）包装成一个任务并放入任务队列。它返回一个 **`std::future`** 对象，可以用于获取任务的执行结果或等待任务完成。这是一个模板函数，支持各种类型的可调用对象和参数。 |
| `void resize_thread_count(const unsigned int new_count)`     | **调整线程数量**。用于调整线程池中的工作线程数量，通常是增加线程数。它可能会抛出 **`ThreadCountUnderflow`** 或 **`ThreadCountOverflow`** 异常，取决于 `new_count` 的值是否过小或过大。 |
| `void set_thread_max_count(const unsigned int cnt)`          | **设置最大线程数**。设置线程池允许的最大工作线程数量。       |
| `void set_thread_min_count(const unsigned int cnt)`          | **设置最小线程数**。设置线程池配置的最小工作线程数量。       |
| `void shutdown_all()`                                        | **停止所有线程**。优雅地关闭所有工作线程，并等待它们结束。   |

| 类型/别名                             | 签名                                                         | 说明                                                         |
| ------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **模板别名** `FutureWrapType`         | `template <class Funtor, class... RequestArguments> using FutureWrapType = ...` | 用于计算给定 **`Funtor`** 及其 **`RequestArguments`** 被调用后返回值的实际类型。它通过 `std::invoke_result_t` 实现，是 `enTask` 返回的 `std::future` 模板参数类型。 |
| **私有类型别名** `CCThreadPoolTask_t` | `using CCThreadPoolTask_t = std::function<void()>;`          | 定义了线程池中任务的基本类型，即一个不接受参数且无返回值的可调用函数对象。 |

很好，让我们从构造开始。

```cpp
CCThreadPool::CCThreadPool(
    const std::unique_ptr<ThreadCountAccessibleProvider> provider) {

	const auto init_cnt = provider->getThreadInitCount();
	thread_max_count = provider->getThreadMaxCount();
	thread_min_count = provider->getThreadMinCount();

	start_worker(init_cnt);
}

virtual ~CCThreadPool() {
	shutdown_all();
}
```

​	`start_worker`和`shutdown_all`是我们的核心。start_worker需要启动起来咱们的所有工作线程。工作线程存储在的地方显然是动态的vector当中，不然我们无法快速的调整我们的线程内容。当然，为了并发安全，我们还需要一个锁来保护我们的代码

```cpp
	std::vector<std::thread> thread_workers; ///< workers for the thread
	std::mutex thread_workers_locker; ///< locker for operating the thread pool
```

​	start_worker应当是直接激活我们的线程开始预备工作。那么，我们的线程如上面所说——有任务执行任务，没有任务就休眠。所以这就说明了几个事情。

- 任务必须是可以传递还可控。我们需要传递函数指针（或者说工作流起始地址），在现代C++中利用std::function就能做到，这是笔者后面选择的抽象
- 任务本身有可能需要被堆积（所有的线程都在忙，不可能阻塞到其他忙完了再去取东西，这是不现实的），所以一个缓冲队列是需要的，我们直接向队列中扔我们的任务，走掉执行我们的其他逻辑。

```cpp
	using CCThreadPoolTask_t = std::function<void()>;
	std::queue<CCThreadPoolTask_t> cached_tasks; ///< tasks queues
	std::mutex tasks_queue_locker; ///< locker for operating the queue
```

​	这就是我们做的抽象，欸？为什么是`std::function<void()>`，不是说好的任意返回任意参数接受嘛？别急，因为这个Task_t是我们包装起来的任务而不是任务本身。我们包装的任务是

```cpp
void worker_func();
```

​	worker_func会查看当然是否有任务值得执行。如果有，那么从我们上面谈到的缓冲队列中索取一个任务，否则的话就需要休眠。这样的话，我们还需要一个信号量

```cpp
std::condition_variable wakeup_cond_var; ///< controlling the wakeups
```

​	现在我们来看如何激活线程：

```cpp
void CCThreadPool::start_worker(const unsigned int sz) {
	std::unique_lock<std::mutex> thread_locker(thread_workers_locker);
	for (unsigned int i = 0; i < sz; i++) {
		// for each tasks, we need to run the self functions
		thread_workers.emplace_back(std::bind(&CCThreadPool::worker_func, this));
	}
}
```

​	线程的激活必须要推入我们自己定义的worker_func。work_func是我们的核心

```cpp
void CCThreadPool::worker_func() {
	CCThreadPoolTask_t task_type;
	while (1) {
		// lock the code to see if
		// 1. there are tasks availables
		// 2. wakeup_cond_var will wake up the thread if tasks availables
		// 3. then we should see if these is due to terminate_self
		// 4. 	if terminate_self == true, then all thread pool should shut down
		{
			std::unique_lock<std::mutex> _locker(tasks_queue_locker);
			wakeup_cond_var.wait(
			    _locker,
			    [this]() {
				    return terminate_self || // indicate terminates
				        !cached_tasks.empty(); // comes the new sessions
			    });
			if (cached_tasks.empty()) {
				if (terminate_self) {
					break;
				} else {
					continue; // continue the sessions
				}
			}
			// get the task
			task_type = std::move(cached_tasks.front());
			cached_tasks.pop();
		}
		if (is_exit_functor(task_type)) {
			// NULL, as we dont owns anything worth execute
			// as this is the actual exit token
			// we should never execute this
			break;
		}
		// invoke the task
		task_type(); // invoke the task
	}
}
```

​	当然，略去退出的部分，实际上就是有任务执行任务，其他的逻辑我们马上就说：

​	对于退出逻辑，我们实际上就是，让退出标志位置1，而且为了放置仿佛调用，我们要做的事情是放置反复做——所以terminate_self是真就立马返回，为了终止线程，我们立马往里面放置空的functor，让他们退出（放置混淆没有任务和退出逻辑）

```cpp
void CCThreadPool::shutdown_all() {
	{
		std::unique_lock<std::mutex> _t(tasks_queue_locker);
		if (terminate_self) // already terminates
			return;
		terminate_self = true;
		std::unique_lock<std::mutex> _w(thread_workers_locker);
		for (auto i = 0; i < thread_workers.size(); i++)
			emplace_exit_functor(); // 放置空的functor，这样线程就能退出执行的内容
	}

	wakeup_cond_var.notify_all(); // 把大家都叫醒
	std::unique_lock<std::mutex> _w(thread_workers_locker);
	for (auto& thread : thread_workers) {
		if (thread.joinable())
			thread.join(); // 强迫等待所有的线程都结束
		// If the thread is cancelled, thats OK!
		// As we have detached it :)
	}

	thread_workers.clear();
}
```

​	`emplace_exit_functor`实际上就是放置空的functor，这样线程就能退出执行的内容，对称的`is_exit_functor`就是检查是不是要退出。

​	剩下的一个就是resize逻辑了：

```cpp
void resize_thread_count(const unsigned int new_count);
```

​	resize很简单，如果扩大，就进一步放入更多的线程，反之，立马插入对应个数的空执行体，直接让对应的线程弹出。

```cpp
// Resize thread pool to n threads (can be larger or smaller)
void CCThreadPool::resize_thread_count(const unsigned int n) {
	if (n < thread_min_count)
		throw ThreadCountUnderflow(n, thread_min_count, thread_max_count);

	if (n > thread_max_count)
		throw ThreadCountOverflow(n, thread_min_count, thread_max_count);

	std::unique_lock<std::mutex> lk(thread_workers_locker);
	size_t current = thread_workers.size();
	if (n == current)
		return; // We dont need to make threads
	if (n > current) {
		// add threads
		auto adder = n - current;
		lk.unlock();
		start_worker(adder);
	} else {
		// reduce threads: enqueue 'null' tasks (stop tokens) so that some workers exit
		size_t remove_cnt = current - n;

		{
			std::unique_lock<std::mutex> qlk(tasks_queue_locker);
			for (size_t i = 0; i < remove_cnt; ++i) {
				// push an empty std::function as exit token
				emplace_exit_functor();
			}
		}

		// wake up all to let them pick up exit tokens
		wakeup_cond_var.notify_all();
		return; // Clean at the close cast
	}
}
```

#### 提交任务

​	最重要的是提交任务：

```cpp
	template <class Funtor, class... RequestArguments>
	auto enTask(Funtor&& functor, RequestArguments&&... requestArgs)
	    -> std::future<FutureWrapType<Funtor, RequestArguments...>> {

		auto task_lambda = [functor = std::forward<Funtor>(functor),
		                    args_tuple = std::make_tuple(std::forward<RequestArguments>(requestArgs)...)]() mutable {
			// std::apply invokes the captured functor with arguments
			// from the tuple.
			// 'mutable' is necessary in case the functor's
			// operator() is not const.
			// MoveOnly& for the args_tuple invoke
			return std::apply(functor, std::move(args_tuple));
		};

		using Result_t = FutureWrapType<Funtor, RequestArguments...>;
		auto runnable_task = std::make_shared<
		    std::packaged_task<
		        Result_t()>>(
		    std::move(task_lambda));

		std::future<Result_t> future = runnable_task->get_future();

		{
			std::unique_lock<std::mutex> lk(tasks_queue_locker);
			if (terminate_self)
				throw ThreadPoolTerminateError();

			// wrap as std::function<void()> by moving shared_ptr into lambda
			cached_tasks.emplace([runnable_task]() { (*runnable_task)(); });
		}

		wakeup_cond_var.notify_one(); // wake up one to finish the sessions
		return future;
	}

```

​	这个函数实际上将任意可调用对象（functor）和若干实参打包成一个**可排队执行的任务**，把任务放进一个任务队列（`cached_tasks`），返回一个 `std::future` 给调用者，调用者可以通过这个 `future` 在任务执行完后拿到结果或异常。

------

#### step1: 构建任务的 lambda（捕获与 `std::apply`）

```cpp
auto task_lambda = [functor = std::forward<Funtor>(functor),
                    args_tuple = std::make_tuple(std::forward<RequestArguments>(requestArgs)...)]() mutable {
    return std::apply(functor, std::move(args_tuple));
};
```

1. `[...]()`：这是带**初始化捕获**（init-capture）的 lambda（C++14 起支持），把传入的 `functor` 和参数打包进 lambda 的闭包中。
   - `functor = std::forward<Funtor>(functor)`：把外层参数完美转发并在闭包里以值的方式持有（如果传入的是右值则移动进来）。
   - `args_tuple = std::make_tuple(std::forward<RequestArguments>(requestArgs)...)`：把所有参数通过 `std::make_tuple` 打包。`make_tuple` 会把参数 **decay/复制/移动** 成 tuple 中的元素类型（即 `std::decay` 行为：去引用、去 cv、将数组/函数折叠为指针等）。
     - 这意味着：如果传入的是一个右值可移动对象，则 `make_tuple` 会把它移动到 tuple；如果传入左值，则会拷贝左值。
     - 与 `std::forward_as_tuple` 不同：`forward_as_tuple` 会保存引用（可能造成后续悬空），而 `make_tuple` 存储的是**owning** 值，这通常是线程/队列场景的正确选择。
2. `mutable`：允许 lambda 在调用时修改捕获的成员（例如 `functor`、`args_tuple`），必要时可移动 `args_tuple`（见下一行）。
3. `std::apply(functor, std::move(args_tuple))`：
   - `std::apply`（C++17）把 tuple 展开并以元素作为参数来调用 `functor`（相当于 `functor(tuple_element0, tuple_element1, ...)`）。
   - 这里使用 `std::move(args_tuple)`，把 tuple 整体移动到 `std::apply` 中——对于 move-only 类型（如 `std::unique_ptr`）支持良好：它们会在任务执行时被移动进实际调用中。
   - 因为 `args_tuple` 是 lambda 的成员，这里移动意味着该 lambda 很可能**只能被调用一次**（因为它把所有参数都移动走了）。在任务场景下，这正是期望的（每个任务只运行一次）。

------

#### step2: 包装成 `std::packaged_task` 和 `std::future`

```cpp
using Result_t = FutureWrapType<Funtor, RequestArguments...>;
auto runnable_task = std::make_shared<std::packaged_task<Result_t()>>(std::move(task_lambda));
std::future<Result_t> future = runnable_task->get_future();
```

- `std::packaged_task<Result_t()>`：把 `task_lambda`（一个可调用对象，签名为 `Result_t()`）封装为一个 **可调用的任务对象**，其内部维护一个 `std::promise`，当任务被调用时会把返回值设置到这个 promise，从而能让获得的 `std::future` 接收结果或异常。
- `std::make_shared<...>`：将 `packaged_task` 放到 `shared_ptr` 中。之所以用 `shared_ptr`，是因为后面在将任务加入队列时，队列里存放的是 `std::function<void()>`（或类似），这个 `std::function` 需要一个可复制的、可延长生命周期的函数对象来调用 `packaged_task`。`std::shared_ptr` 保证即使原函数栈返回，被封装的 `packaged_task` 依然存活直到队列中的 lambda 被调用完成。
- `runnable_task->get_future()`：得到与 `packaged_task` 关联的 `std::future`，并返回给调用者，让其等待或获取结果。

注意：`std::packaged_task` 的 `get_future()` 必须在 `packaged_task` 还存在且只调用一次（规范要求）。这里先得到 `future` 是正确的顺序。

------

#### step3: 推入任务队列（线程安全）并唤醒 worker

```cpp
{
    std::unique_lock<std::mutex> lk(tasks_queue_locker);
    if (terminate_self)
        throw ThreadPoolTerminateError();

    cached_tasks.emplace([runnable_task]() { (*runnable_task)(); });
}

wakeup_cond_var.notify_one();
return future;
```

- `std::unique_lock<std::mutex> lk(tasks_queue_locker);`：用互斥锁保护对 `cached_tasks` 的访问（线程安全）。
- `if (terminate_self) throw ThreadPoolTerminateError();`：如果线程池正在终止，不接受新任务，抛出异常。
- `cached_tasks.emplace([runnable_task]() { (*runnable_task)(); });`：
  - 把一个 `std::function<void()>`（或可调用类型）放入任务队列，这个 lambda 捕获了 `shared_ptr` `runnable_task`（按值捕获），并在 worker 执行时调用 `(*runnable_task)()`，从而触发 `packaged_task` 把结果/异常传递给 `future`。
  - 使用 `shared_ptr` 的好处：队列里仅存放一个小的复制（`shared_ptr` 增加引用计数），避免移动或析构问题。
- `wakeup_cond_var.notify_one();`：唤醒至少一个等待的 worker 线程，让其取出并执行刚放入的任务。
- `return future;`：将 `future` 返回给调用者。

