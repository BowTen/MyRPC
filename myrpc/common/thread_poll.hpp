#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace myrpc {

class ThreadPool {
public:
	using ptr = std::shared_ptr<ThreadPool>;
	ThreadPool(size_t threads) : stop(false), numThreads(threads) {
		for (size_t i = 0; i < threads; ++i) {
			workers.emplace_back([this] {
				while (true) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(queue_mutex);
						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
						if (this->stop && this->tasks.empty()) return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}
					task();
				}
			});
		}
	}

	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread &worker : workers) {
			worker.join();
		}
	}
	void enqueue(std::function<void()> task) {
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			tasks.emplace(std::move(task));
		}
		condition.notify_one();
	}
	int getThreadNum() {
		return numThreads;
	}

private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
	int numThreads;
};

}