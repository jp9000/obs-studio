/*
 * Based on Loki::ScopeGuard
 */

#pragma once

#include <utility>

namespace scope_guard_util {

template<typename FunctionType> class ScopeGuard {
public:
	void dismiss() noexcept { dismissed_ = true; }

	explicit ScopeGuard(const FunctionType &fn) : function_(fn) {}

	explicit ScopeGuard(FunctionType &&fn) : function_(std::move(fn)) {}

	ScopeGuard(ScopeGuard &&other)
		: dismissed_(other.dismissed_),
		  function_(std::move(other.function_))
	{
		other.dismissed_ = true;
	}

	~ScopeGuard() noexcept
	{
		if (!dismissed_)
			execute();
	}

private:
	void *operator new(size_t) = delete;

	void execute() noexcept { function_(); }

	bool dismissed_ = false;
	FunctionType function_;
};

template<typename FunctionType>
ScopeGuard<typename std::decay<FunctionType>::type>
make_guard(FunctionType &&fn)
{
	return ScopeGuard<typename std::decay<FunctionType>::type>{
		std::forward<FunctionType>(fn)};
}

namespace detail {

enum class ScopeGuardOnExit {};

template<typename FunctionType>
ScopeGuard<typename std::decay<FunctionType>::type>
operator+(detail::ScopeGuardOnExit, FunctionType &&fn)
{
	return ScopeGuard<typename std::decay<FunctionType>::type>(
		std::forward<FunctionType>(fn));
}

}

} // namespace scope_guard_util

#define SCOPE_EXIT_CONCAT2(x, y) x##y
#define SCOPE_EXIT_CONCAT(x, y) SCOPE_EXIT_CONCAT2(x, y)
#define SCOPE_EXIT                                           \
	auto SCOPE_EXIT_CONCAT(SCOPE_EXIT_STATE, __LINE__) = \
		::scope_guard_util::detail::ScopeGuardOnExit() + [&]() noexcept
