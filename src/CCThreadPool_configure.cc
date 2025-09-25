/**
 * @file CCThreadPool_configure.cc
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief configuring the threadpool relatives utils cc
 * @version 0.1
 * @date 2025-09-25
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "CCThreadPool.h"
#include "CCThreadPoolError.h"
#include <thread>

using namespace CCThreadPool;

/**
 * @brief MIN_ACCEPT_THREAD shows the pass check limits for user provides
 *
 */
static constexpr const unsigned int MIN_ACCEPT_THREAD = 1;
static_assert(MIN_ACCEPT_THREAD >= 1, "Thread Count should be literal ge than 1");

unsigned int ThreadCountDefaultProvider::provideThreadInitCount() const noexcept {
	return std::thread::hardware_concurrency() / 2;
}

unsigned int ThreadCountDefaultProvider::provideThreadMaxCount() const noexcept {
	return std::thread::hardware_concurrency();
}
unsigned int ThreadCountDefaultProvider::provideThreadMinCount() const noexcept {
	return 1;
}

namespace {
/**
 * @brief
 *
 * @param given_cnt
 * @param min_count
 * @param max_count
 * @return true
 * @return false
 */
inline bool trigger_check(const int given_cnt,
                          const int min_count,
                          const int max_count) {
	return min_count > given_cnt || given_cnt > max_count;
}

};

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
	if (cnt < MIN_ACCEPT_THREAD) {
		throw ThreadCountRangeError(cnt, MIN_ACCEPT_THREAD);
	}
	return cnt;
}