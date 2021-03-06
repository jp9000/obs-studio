#include <windows.h>
#include <stdbool.h>
#include "sleepto.h"

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;

static inline uint64_t get_clockfreq(void)
{
	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}

	return clock_freq.QuadPart;
}

uint64_t gettime_100ns(void)
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 10000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}

bool sleepto_100ns(uint64_t time_target)
{
	uint64_t t = gettime_100ns();
	uint32_t milliseconds;

	if (t >= time_target)
		return false;

	milliseconds = (uint32_t)((time_target - t) / 10000);
	if (milliseconds > 1)
		Sleep(milliseconds - 1);

	for (;;) {
		t = gettime_100ns();
		if (t >= time_target)
			return true;

		Sleep(0);
	}
}
