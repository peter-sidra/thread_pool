#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
  private:
	std::mutex tasks_mutex;
	std::vector<std::thread> threads;
	std::counting_semaphore<PTRDIFF_MAX> semaphore;
	std::queue<std::function<void()>> tasks;
	std::condition_variable work_done_cv;

	ptrdiff_t max_tasks;
	bool terminate_pool = false;

  public:
	explicit ThreadPool(
		unsigned long long num_threads = std::thread::hardware_concurrency(),
		ptrdiff_t max_tasks = PTRDIFF_MAX)
		: semaphore(0), max_tasks(max_tasks) {
		num_threads = std::min(num_threads, (unsigned long long)max_tasks);

		for (unsigned long long i = 0; i < num_threads; i++) {
			threads.emplace_back([this] {
				while (true) {
					semaphore.acquire();

					// get a task from the queue
					std::function<void()> task;
					{
						std::lock_guard<std::mutex> threads_lock_guard{
							tasks_mutex};
						if (terminate_pool && tasks.empty()) {
							return;
						}
						task = tasks.front();
						tasks.pop();
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
	auto push_task(F &&task, Args... args) -> std::future<R> {
		// block if the pool is at capacity
		{
			std::unique_lock uniqueLock(tasks_mutex);
			work_done_cv.wait(uniqueLock, [this] {
				return tasks.size() < (size_t)max_tasks;
			});
		}

		auto packaged_task = std::make_shared<std::packaged_task<R()>>(
			std::bind(std::forward<F>(task), std::forward<Args>(args)...));

		auto fut = packaged_task->get_future();

		{
			std::lock_guard<std::mutex> threads_lock_guard{tasks_mutex};
			tasks.emplace([packaged_task = std::move(packaged_task)] {
				(*packaged_task)();
			});
		}

		semaphore.release();
		return fut;
	}

	auto wait_all() {
		std::unique_lock uniqueLock(tasks_mutex);

		work_done_cv.wait(uniqueLock, [this] { return tasks.empty(); });
	}

	~ThreadPool() {
		terminate_pool = true;
		semaphore.release(static_cast<int>(threads.size()));
		for (auto &thread : threads) {
			thread.join();
		}
	}
};
