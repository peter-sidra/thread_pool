#include "thread_pool/thread_pool.hpp"
#include <future>
#include <iostream>
#include <string>
#include <vector>

auto do_work(size_t index) -> std::string {
	std::cout << "Doing work for task: " << index << "\n";
	return "This is the result of task: " + std::to_string(index);
}

auto main() -> int {
	thread_pool::ThreadPool pool;

	constexpr size_t EXAMPLE_SIZE = 500;

	std::vector<std::future<std::string>> futures;
	futures.reserve(EXAMPLE_SIZE);

	for (size_t i = 0; i < EXAMPLE_SIZE; ++i) {
		futures.emplace_back(pool.push_task(do_work, i));
	}

	for (auto &fut : futures) {
		std::cout << fut.get() << '\n';
	}

	return 0;
}