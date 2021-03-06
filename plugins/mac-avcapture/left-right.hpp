#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include <cinttypes>

// left_right based on
// https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/left-right-2014.pdf
// see concurrencyfreaks.com

namespace left_right {

template<typename T> struct left_right {
	template<typename Func> void update(Func &&f)
	{
		std::lock_guard<std::mutex> lock(write_mutex);
		auto cur = current.load();
		auto next = (cur + 1) & 1;

		while (readers[next] != 0)
			std::this_thread::yield();

		f(data[next]);
		current = next;

		while (readers[cur] != 0)
			std::this_thread::yield();

		f(data[cur]);
	}

	T read()
	{
		auto cur = current.load();
		for (;;) {
			readers[cur] += 1;

			auto cur_ = current.load();
			if (cur_ == cur)
				break;

			readers[cur] -= 1;
			cur = cur_;
		}

		auto val = data[cur];

		readers[cur] -= 1;

		return val;
	}

private:
	std::atomic_uint_fast8_t current;
	std::atomic_long readers[2];
	std::mutex write_mutex;

	T data[2] = {{}, {}};
};

}
