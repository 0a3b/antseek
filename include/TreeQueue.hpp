#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_set>

// Thread-safe queue for multi-threaded tree structure processing (e.g., file system traversal)
// Calling pop() should only start after at least one element has been pushed into the queue
// pop() returns false if the queue is empty and all threads have finished their work, meaning the entire processing is complete
template<typename TValue>
class TreeQueue {
private:
    std::queue<TValue> taskQueue;
    std::mutex mtx;
    std::condition_variable_any cv;
    std::atomic<int> threadsWaitingForTasks{ 0 };
    int numberOfThreads;
    bool allThreadsCompleted{ false };

public:
	TreeQueue() = delete;
	TreeQueue(int numThreads) : numberOfThreads(numThreads) {} // It must be aware of the exact number of threads that will be using the queue's pop function.

    void push(const TValue& path) {
        {
            std::lock_guard lock(mtx);
            taskQueue.push(path);
        }
        cv.notify_one();
    }

    bool pop(TValue& out, std::stop_token stopToken) {
        std::unique_lock lock(mtx);
        ++threadsWaitingForTasks;
        if (threadsWaitingForTasks >= numberOfThreads) {
            allThreadsCompleted = true;
            cv.notify_all();
        }
        cv.wait(lock, stopToken, [this] { return !taskQueue.empty() || allThreadsCompleted; });
        --threadsWaitingForTasks;

        if (taskQueue.empty() || stopToken.stop_requested()) {
            return false;
        }

        out = taskQueue.front();
        taskQueue.pop();
        return true;
    }
};