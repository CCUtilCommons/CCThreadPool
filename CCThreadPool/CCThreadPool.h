/**
 * @file CCThreadPool.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief   CCThreadPool Interface definitions, these is aims to
 *          provide an interface support for CCThreadPool, You can later
 *          wrap your own instance
 * @version 0.1
 * @date 2025-09-25
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include "CCThreadPoolError.h"
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
namespace CCThreadPool {

/**
 * @brief   ThreadCountAccessibleProvider provide how many thread
 *          should we get for the configures count;
 *
 */
struct ThreadCountAccessibleProvider {
	virtual unsigned int provideThreadInitCount() const noexcept = 0;
	virtual unsigned int provideThreadMaxCount() const noexcept = 0;
	virtual unsigned int provideThreadMinCount() const noexcept = 0;
	virtual ~ThreadCountAccessibleProvider() = default;

private:
	friend class CCThreadPool;
	unsigned int getThreadInitCount() const; ///< interative interfaces
	unsigned int getThreadMaxCount() const; ///< interative interfaces
	unsigned int getThreadMinCount() const; ///< interative interfaces
}; // ThreadCountAccessibleProvider

struct ThreadCountDefaultProvider : ThreadCountAccessibleProvider {
	unsigned int provideThreadInitCount() const noexcept override;
	unsigned int provideThreadMaxCount() const noexcept override;
	unsigned int provideThreadMinCount() const noexcept override;
};

class CCThreadPool {
public:
	CCThreadPool(
	    const std::unique_ptr<ThreadCountAccessibleProvider> provider
	    = std::make_unique<ThreadCountDefaultProvider>());

	~CCThreadPool() {
		shutdown_all();
	}
	template <class Funtor, class... RequestArguments>
	using FutureWrapType = std::invoke_result_t<
	    std::decay_t<Funtor>,
	    std::decay_t<RequestArguments>...>;

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

	/**
	 * @brief   resize_thread_count will resize the thread counts up!
	 *          to be noticed: these shell throw exceptions
	 * @exception   ThreadCountUnderflow means the resize count your refer
	 *              to small
	 *              ThreadCountOverflow  means the resize count your refer
	 *              to large
	 *
	 * @param new_count the count of new
	 */
	void resize_thread_count(const unsigned int new_count);

	/**
	 * @brief Set the thread max count object
	 *
	 * @param cnt
	 */
	void set_thread_max_count(const unsigned int cnt);

	/**
	 * @brief Set the thread min count object
	 *
	 * @param cnt
	 */
	void set_thread_min_count(const unsigned int cnt);

	void shutdown_all(); ///< shutup, threads!
private:
	std::vector<std::thread> thread_workers; ///< workers for the thread
	std::mutex thread_workers_locker; ///< locker for operating the thread pool

	using CCThreadPoolTask_t = std::function<void()>;
	std::queue<CCThreadPoolTask_t> cached_tasks; ///< tasks queues
	std::mutex tasks_queue_locker; ///< locker for operating the queue

	std::condition_variable wakeup_cond_var; ///< controlling the wakeups

	bool terminate_self { false };

private:
	/* ------------ Disable the Default Implementations -------------- */
	CCThreadPool()
	    = delete; // we must disabled this for force assigned
	CCThreadPool(const CCThreadPool&) = delete;
	CCThreadPool& operator=(const CCThreadPool&) = delete;

	/* ------------ Some Helpers ------------ */
	void start_worker(const unsigned int sz); ///< init the worker given by the

	void worker_func();

	virtual bool is_exit_functor(const CCThreadPoolTask_t& functor) const;
	virtual void emplace_exit_functor();

	unsigned int thread_min_count; ///< configured by the package
	unsigned int thread_max_count; ///< configured by the package
}; // defines the CCThreadPool

} // namespace CCThreadPool
