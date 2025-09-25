#include "CCThreadPool.h"
#include "CCThreadPoolError.h"
#include <mutex>

namespace CCThreadPool {

CCThreadPool::CCThreadPool(
    const std::unique_ptr<ThreadCountAccessibleProvider> provider) {

	const auto init_cnt = provider->getThreadInitCount();
	thread_max_count = provider->getThreadMaxCount();
	thread_min_count = provider->getThreadMinCount();

	start_worker(init_cnt);
}

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

void CCThreadPool::start_worker(const unsigned int sz) {
	std::unique_lock<std::mutex> thread_locker(thread_workers_locker);
	for (unsigned int i = 0; i < sz; i++) {
		// for each tasks, we need to run the self functions
		thread_workers.emplace_back(std::bind(&CCThreadPool::worker_func, this));
	}
}

void CCThreadPool::shutdown_all() {
	{
		std::unique_lock<std::mutex> _t(tasks_queue_locker);
		if (terminate_self) // already terminates
			return;
		terminate_self = true;
		std::unique_lock<std::mutex> _w(thread_workers_locker);
		for (auto i = 0; i < thread_workers.size(); i++)
			emplace_exit_functor();
	}

	wakeup_cond_var.notify_all();
	std::unique_lock<std::mutex> _w(thread_workers_locker);
	for (auto& thread : thread_workers) {
		if (thread.joinable())
			thread.join();
		// If the thread is cancelled, thats OK!
		// As we have detached it :)
	}

	thread_workers.clear();
}

bool CCThreadPool::is_exit_functor(const CCThreadPoolTask_t& functor) const {
	return !functor; // NULL Functor
}

void CCThreadPool::emplace_exit_functor() {
	cached_tasks.emplace(CCThreadPoolTask_t());
}

void CCThreadPool::worker_func() {
	CCThreadPoolTask_t task_type;
	while (1) {
		/**
		 * @brief First step, we need to
		 *
		 */
		{ // For assigned task locker
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

			task_type = std::move(cached_tasks.front());
			cached_tasks.pop();
			if (!task_type) {
				// NULL, as we dont owns anything worth execute
				// as this is the actual exit token
				// we should never execute this
				break;
			}
			// invoke the task
			task_type(); // invoke the task
		}
	}
}

}; // CCThreadPool namespace
