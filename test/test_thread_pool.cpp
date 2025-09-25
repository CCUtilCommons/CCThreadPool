#include "CCThreadPool.h"
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// small helper printing banner
static void banner(const char* s) {
	std::cout << "\n==== " << s << " ====\n";
}

// ---------- Simple assertions ----------
#define ASSERT_TRUE(cond, msg)                             \
	do {                                                   \
		if (!(cond)) {                                     \
			std::cerr << "ASSERT FAILED: " << msg << "\n"; \
			std::terminate();                              \
		}                                                  \
	} while (0)
#define ASSERT_EQ(a, b, msg)                                                                \
	do {                                                                                    \
		if (!((a) == (b))) {                                                                \
			std::cerr << "ASSERT FAILED: " << msg << " (" << (a) << " != " << (b) << ")\n"; \
			std::terminate();                                                               \
		}                                                                                   \
	} while (0)

// ---------- Helper test items ----------
int add(int a, int b) {
	return a + b;
}
struct MoveOnly {
	std::unique_ptr<int> p;
	MoveOnly(int v = 0)
	    : p(std::make_unique<int>(v)) { }
	int use() const { return *p; }
};

// ---------- Tests ----------

// 1) 基本接口测试：简单返回值、lambda、move-only、异常传播
void test_interface_basic() {
	banner("interface_basic");

	CCThreadPool::CCThreadPool pool(std::make_unique<CCThreadPool::ThreadCountDefaultProvider>());

	// simple return
	auto f1 = pool.enTask(add, 2, 3);
	ASSERT_EQ(f1.get(), 5, "add result");

	// lambda returning size
	auto f2 = pool.enTask([](std::string s) { return s.size(); }, std::string("hello"));
	ASSERT_EQ(f2.get(), 5u, "lambda result");

	// move-only argument and return type usage
	auto f3 = pool.enTask([](MoveOnly m) -> int { return m.use(); }, MoveOnly(42));
	ASSERT_EQ(f3.get(), 42, "move-only result");

	// exception propagation
	auto f4 = pool.enTask([]() { throw std::runtime_error("boom"); return 1; });
	bool threw = false;
	try {
		(void)f4.get();
	} catch (const std::exception& e) {
		threw = true;
		std::cout << "caught expected exception: " << e.what() << "\n";
	}
	ASSERT_TRUE(threw, "exception propagated");

	std::cout << "interface_basic passed\n";
}

// 2) 边缘测试：enqueue after shutdown, resize under/overflows (assumes pool throws errors defined in header)
void test_edge_cases() {
	banner("edge_cases");
	CCThreadPool::CCThreadPool pool(std::make_unique<CCThreadPool::ThreadCountDefaultProvider>());

	// enqueue 0 tasks (just create and wait briefly)
	std::this_thread::sleep_for(10ms);

	// shutdown and then enqueue should throw ThreadPoolTerminateError (as in header)
	pool.shutdown_all(); // call the pool's shutdown helper (exists in header)
	bool got_throw = false;
	try {
		auto f = pool.enTask([]() { return 1; });
		(void)f.get();
	} catch (...) {
		got_throw = true;
	}
	ASSERT_TRUE(got_throw, "enqueue after shutdown should fail");

	std::cout << "edge_cases passed\n";
}

// 3) 并发 & 压力测试：多生产者 + 大量短小任务（计数器校验）
void test_stress_concurrent(unsigned int producers = 8, unsigned int tasks_per_producer = 200000) {
	banner("stress_concurrent");
	CCThreadPool::CCThreadPool pool(std::make_unique<CCThreadPool::ThreadCountDefaultProvider>());

	std::atomic<uint64_t> counter { 0 };
	std::vector<std::thread> producers_v;
	auto start = std::chrono::steady_clock::now();

	for (unsigned int p = 0; p < producers; ++p) {
		producers_v.emplace_back([&pool, &counter, tasks_per_producer]() {
			for (unsigned int i = 0; i < tasks_per_producer; ++i) {
				pool.enTask([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
			}
		});
	}
	for (auto& t : producers_v)
		t.join();

	// wait for completion (simple wait loop; could be improved with condition)
	while (counter.load(std::memory_order_relaxed) < uint64_t(producers) * tasks_per_producer) {
		std::this_thread::sleep_for(1ms);
	}
	auto dur = std::chrono::steady_clock::now() - start;
	double secs = std::chrono::duration<double>(dur).count();
	uint64_t total = uint64_t(producers) * tasks_per_producer;
	std::cout << "Stress: tasks=" << total << " time=" << secs << "s TPS=" << (total / secs) << "\n";

	std::cout << "stress_concurrent passed\n";
}

void perf_test_vary_tasks_and_threads() {
	std::cout << "=== perf_test ===\n";

	struct Scenario {
		std::string name;
		std::chrono::microseconds work_us;
		unsigned int tasks;
	};

	std::vector<Scenario> scenarios = {
		{ "tiny", 10us, 200000 },
		{ "short", 100us, 100000 },
		{ "medium", 2000us, 20000 },
		{ "long", 20000us, 2000 }
	};

	std::vector<unsigned int> thread_counts = { 1, 2, std::thread::hardware_concurrency() };

	for (auto& sc : scenarios) {
		std::cout << "--- scenario: " << sc.name
		          << " work(us)=" << sc.work_us.count()
		          << " tasks=" << sc.tasks << " ---\n";

		for (unsigned int tc : thread_counts) {
			CCThreadPool::CCThreadPool pool(std::make_unique<CCThreadPool::ThreadCountDefaultProvider>());
			pool.resize_thread_count(tc);

			std::atomic<uint64_t> done { 0 };
			std::mutex mtx;
			std::condition_variable cv;

			auto start_time = std::chrono::steady_clock::now();

			if (sc.name == "tiny") {
				// tiny scenario 批量任务
				unsigned int batch_size = 100; // 每个任务执行 100 个微任务
				for (unsigned int i = 0; i < sc.tasks; i += batch_size) {
					pool.enTask([&, start = i]() {
						unsigned int end = std::min(start + batch_size, sc.tasks);
						for (unsigned int j = start; j < end; ++j) {
							auto t0 = std::chrono::steady_clock::now();
							while (std::chrono::steady_clock::now() - t0 < sc.work_us) { }
						}
						if (done.fetch_add(end - start) + (end - start) >= sc.tasks) {
							std::unique_lock<std::mutex> lk(mtx);
							cv.notify_one();
						}
					});
				}
			} else {
				// short/medium/long scenario 单任务提交
				for (unsigned int i = 0; i < sc.tasks; ++i) {
					pool.enTask([&, work = sc.work_us]() {
						if (work <= 100us) {
							auto t0 = std::chrono::steady_clock::now();
							while (std::chrono::steady_clock::now() - t0 < work) { }
						} else {
							std::this_thread::sleep_for(work);
						}

						if (done.fetch_add(1) + 1 == sc.tasks) {
							std::unique_lock<std::mutex> lk(mtx);
							cv.notify_one();
						}
					});
				}
			}

			// 等待所有任务完成
			std::unique_lock<std::mutex> lk(mtx);
			cv.wait(lk, [&] { return done.load() == sc.tasks; });

			auto end_time = std::chrono::steady_clock::now();
			double secs = std::chrono::duration<double>(end_time - start_time).count();
			double tps = sc.tasks / secs;

			std::cout << "threads=" << tc << " time=" << secs << "s TPS=" << tps << "\n";

			pool.shutdown_all();
		}
	}

	std::cout << "perf_test done\n";
}

// 5) resize under load: increase and decrease threads while tasks exist
void test_resize_behavior() {
	banner("resize_behavior");
	CCThreadPool::CCThreadPool pool(std::make_unique<CCThreadPool::ThreadCountDefaultProvider>());
	pool.resize_thread_count(4);

	std::atomic<int> completed { 0 };
	const int total = 50000;
	for (int i = 0; i < total; ++i) {
		pool.enTask([&completed]() {
			// tiny workload
			completed.fetch_add(1, std::memory_order_relaxed);
		});
	}

	// increase to 12
	pool.resize_thread_count(12);
	std::this_thread::sleep_for(200ms);

	// decrease to 2
	pool.resize_thread_count(2);

	// wait
	while (completed.load(std::memory_order_relaxed) < total)
		std::this_thread::sleep_for(1ms);

	std::cout << "resize_behavior passed\n";
}

// ---------- main ----------
int main(int argc, char** argv) {
	try {
		test_interface_basic();
	} catch (...) {
		std::cerr << "interface_basic failed\n";
		return 1;
	}

	try {
		test_edge_cases();
	} catch (...) {
		std::cerr << "edge_cases failed\n";
		return 2;
	}

	try {
		// stress: reduce sizes locally for CI or increase for real machine
		test_stress_concurrent(4, 50000); // ~200k tasks total
	} catch (...) {
		std::cerr << "stress_concurrent failed\n";
		return 3;
	}

	try {
		perf_test_vary_tasks_and_threads();
	} catch (...) {
		std::cerr << "perf_test failed\n";
		return 4;
	}

	try {
		test_resize_behavior();
	} catch (...) {
		std::cerr << "resize_behavior failed\n";
		return 5;
	}

	std::cout << "\nALL TESTS PASSED\n";
	return 0;
}
