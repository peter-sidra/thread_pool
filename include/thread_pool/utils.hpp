#pragma once

#include <mutex>

namespace thread_pool {

template <typename T> class MutexWrapper {
  private:
	std::mutex mutex;
	T object;

  public:
	class ScopedAccessor {
	  private:
		std::scoped_lock<std::mutex> lock;
		T &object;

	  public:
		ScopedAccessor(T &object, std::mutex &mutex)
			: lock(mutex), object(object) {}

		auto operator->() -> T * {
			return &object;
		}
	};

	template <typename... TArgs>
	MutexWrapper(TArgs &&...args) : object(std::forward<TArgs>(args)...) {}
	MutexWrapper(T &&object) : object(std::forward<T>(object)) {}

	auto getScopedAccessor() -> ScopedAccessor {
		return ScopedAccessor(object, mutex);
	}

	auto getMutex() -> std::mutex & {
		return mutex;
	}

	auto getUnsafeAccessor() -> T & {
		return object;
	}
};

} // namespace thread_pool