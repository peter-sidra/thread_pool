#pragma once

#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "utils.hpp"

namespace thread_pool {

class ThreadPool {
  private:
	std::vector<std::thread> threads;
	MutexWrapper<std::queue<std::function<void()>>> tasks;
	std::condition_variable work_available_cv;
	std::condition_variable work_done_cv;

	size_t max_tasks;
	bool terminate_pool = false;

  public:
	explicit ThreadPool(
		unsigned long long num_threads = std::thread::hardware_concurrency(),
		size_t max_tasks = std::numeric_limits<size_t>::max())
		: max_tasks(max_tasks) {
		num_threads = std::min(num_threads, (unsigned long long)max_tasks);

		for (unsigned long long i = 0; i < num_threads; i++) {
			threads.emplace_back([this] {
				while (true) {
					std::function<void()> task;

					// Block until a task is available
					{
						std::unique_lock<std::mutex> lock(tasks.getMutex());
						work_available_cv.wait(lock, [this] {
							return terminate_pool ||
								   !tasks.getUnsafeAccessor().empty();
						});

						if (terminate_pool &&
							tasks.getUnsafeAccessor().empty()) {
							return;
						}

						// get a task from the queue
						task = std::move(tasks.getUnsafeAccessor().front());
						tasks.getUnsafeAccessor().pop();
					}

					// execute the task
					task();

					// notify the main thread that a task has been completed
					work_done_cv.notify_all();
				}
			});
		}
	}

	// delete copy and move constructors and assign operators
	ThreadPool(const ThreadPool &) = delete;
	ThreadPool(ThreadPool &&other) = delete;
	auto operator=(const ThreadPool &) -> ThreadPool & = delete;
	auto operator=(ThreadPool &&) -> ThreadPool & = delete;

	template <typename F, typename... Args,
			  typename R = typename std::invoke_result<F, Args...>::type>
	auto push_task(F &&task, Args &&...args) -> std::future<R> {
		// block if the pool is at capacity
		{
			std::unique_lock uniqueLock(tasks.getMutex());
			work_done_cv.wait(uniqueLock, [this] {
				return tasks.getUnsafeAccessor().size() < (size_t)max_tasks;
			});
		}

		auto packaged_task = std::make_shared<std::packaged_task<R()>>(
			[task = std::forward<F>(task),
			 ... args = std::forward<Args>(args)] { return task(args...); });

		auto fut = packaged_task->get_future();

		{
			auto tasks_accessor = tasks.getScopedAccessor();
			tasks_accessor->emplace([packaged_task = std::move(packaged_task)] {
				(*packaged_task)();
			});
		}

		// Notify one of the worker threads that a task is available
		work_available_cv.notify_one();
		return fut;
	}

	auto wait_all() {
		std::unique_lock uniqueLock(tasks.getMutex());

		work_done_cv.wait(uniqueLock,
						  [this] { return tasks.getUnsafeAccessor().empty(); });
	}

	~ThreadPool() {
		{
			std::scoped_lock lock(tasks.getMutex());
			terminate_pool = true;
		}
		work_available_cv.notify_all();
		for (auto &thread : threads) {
			thread.join();
		}
	}
};

} // namespace thread_pool