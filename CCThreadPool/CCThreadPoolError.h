/**
 * @file CCThreadPoolError.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief defines the special errors here
 * @version 0.1
 * @date 2025-09-25
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <exception>
#include <stdexcept>
#include <string>

#if __cplusplus <= 202002L
#include <sstream>
#else
#include <format>
#endif

namespace {

inline const std::string __make_error_messages(
    const std::string& prefix,
    int given_cnt,
    int min_count,
    int max_count) {
#if __cplusplus <= 202002L
	std::ostringstream oss;
	oss << prefix << ", Given " << given_cnt
	    << ", but the range is from " << min_count
	    << " to " << max_count;
	return oss.str();
#else
	return std::format(
	    "{}, Given {}, but the range is from {} to {}",
	    prefix, given_cnt, min_count, max_count);
#endif
}

inline const std::string __make_range_error_messages(
    int given_cnt,
    int min_count) {
#if __cplusplus <= 202002L
	std::ostringstream oss;
	oss << "Under limit: Given " << given_cnt
	    << ", but the limit is " << min_count;
	return oss.str();
#else
	return std::format(
	    "Under limit: Given {}, but the limit is {}",
	    given_cnt, min_count);
#endif
}

} // namespace

#define EXCEPT_WHAT_SIGNATURE \
	virtual const char* what() const noexcept override

class ThreadCountOverflow : public std::exception {
public:
	ThreadCountOverflow(int error_count, int min_count, int max_count)
	    : std::exception()
	    , error_count(error_count)
	    , min_count(min_count)
	    , max_count(max_count)
	    , error_message(__make_error_messages(
	          "ThreadCount is Underflow!",
	          error_count,
	          min_count,
	          max_count)) {
	}

	EXCEPT_WHAT_SIGNATURE {
		return error_message.c_str();
	}

private:
	int error_count;
	int min_count;
	int max_count;
	std::string error_message;
};

class ThreadCountUnderflow : public std::exception {
public:
	ThreadCountUnderflow(int error_count, int min_count, int max_count)
	    : std::exception()
	    , error_count(error_count)
	    , min_count(min_count)
	    , max_count(max_count)
	    , error_message(__make_error_messages(
	          "ThreadCount is Underflow!",
	          error_count,
	          min_count,
	          max_count)) {
	}

	EXCEPT_WHAT_SIGNATURE {
		return error_message.c_str();
	}

private:
	int error_count;
	int min_count;
	int max_count;
	std::string error_message;
};

class ThreadCountRangeError : public std::exception {
public:
	ThreadCountRangeError(int error_count, int min_count)
	    : std::exception()
	    , error_count(error_count)
	    , min_count(min_count)
	    , error_message(__make_range_error_messages(error_count, min_count)) {
	}

	EXCEPT_WHAT_SIGNATURE {
		return error_message.c_str();
	}

private:
	int error_count;
	int min_count;
	std::string error_message;
};

class ThreadPoolTerminateError : std::runtime_error {
public:
	ThreadPoolTerminateError()
	    : std::runtime_error("enqueue on stopped ThreadPool") {
	}
};

#undef EXCEPT_WHAT_SIGNATURE // Dont leak the defines