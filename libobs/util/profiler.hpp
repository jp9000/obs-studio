#pragma once

#include "profiler.h"

struct ScopeProfiler {
	const char *name;
	bool enabled = true;

	ScopeProfiler(const char *name) : name(name) { profile_start(name); }

	~ScopeProfiler() { Stop(); }

	ScopeProfiler(const ScopeProfiler &) = delete;
	ScopeProfiler(ScopeProfiler &&other)
		: name(other.name), enabled(other.enabled)
	{
		other.enabled = false;
	}

	ScopeProfiler &operator=(const ScopeProfiler &) = delete;
	ScopeProfiler &operator=(ScopeProfiler &&other) = delete;

	void Stop()
	{
		if (!enabled)
			return;

		profile_end(name);
		enabled = false;
	}
};

#ifndef NO_PROFILER_MACROS

#define ScopeProfiler_NameConcatImpl(x, y) x##y
#define ScopeProfiler_NameConcat(x, y) ScopeProfiler_NameConcatImpl(x, y)

#ifdef __COUNTER__
#define ScopeProfiler_Name(x) ScopeProfiler_NameConcat(x, __COUNTER__)
#else
#define ScopeProfiler_Name(x) ScopeProfiler_NameConcat(x, __LINE__)
#endif

#define ProfileScope(x) \
	ScopeProfiler ScopeProfiler_Name(SCOPE_PROFILE) { x }

#endif
