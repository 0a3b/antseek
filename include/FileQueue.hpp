#pragma once

#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include "HashUtils.hpp"

// A specialized queue for detecting multiple instances of the same file in a filesystem.
// This class implements a special-purpose queue where elements are pushed one by one,
// but can only be popped if their key has occurred multiple times.
template<typename TValue>
class FileQueue {
public:
    template<typename TKey>
    void push(TKey key, const TValue& value) {
        {
            std::lock_guard lock(mtx);
            auto& map = getMap<TKey>();
            auto it = map.find(key);
            if (it != map.end()) {
                auto& valuePair = it->second;
                if (!valuePair.first) {
                    valuePair.first = true;
                    fileQueue.push(valuePair.second);
                }
                fileQueue.push(value);
                cv.notify_one();
            }
            else {
                map[key] = { false, value };
            }
        }
    }

    void pushPassthrough(const TValue& value) {
        {
            std::lock_guard lock(mtx);
            fileQueue.push(value);
        }
        cv.notify_one();
    }

    bool pop(TValue& out, std::stop_token stopToken) {
        std::unique_lock lock(mtx);

        cv.wait(lock, stopToken, [this] { return !fileQueue.empty() || finished; });

        if (fileQueue.empty() || stopToken.stop_requested()) {
            return false;
        }

        out = fileQueue.front();
        fileQueue.pop();
        return true;
    }

    void setFinished() {
        {
            std::lock_guard lock(mtx);
            finished = true;
        }
        cv.notify_all();
    }

private:
    std::unordered_map<std::uintmax_t, std::pair<bool, TValue>> filesBySize;
    std::unordered_map<std::string, std::pair<bool, TValue>> filesByName;
    std::unordered_map<std::pair<std::uintmax_t, std::string>, std::pair<bool, TValue>, HashUtils::pairHash> filesBySizeAndName;

    std::queue<TValue> fileQueue;
    std::mutex mtx;
    std::condition_variable_any cv;
    bool finished{ false }; // Indicates that no more elements will be added, but some elements may still be processing

    template<typename TKey>
    auto& getMap() {
        if constexpr (std::is_same_v<TKey, std::uintmax_t>) {
            return filesBySize;
        }
        else if constexpr (std::is_same_v<TKey, std::string>) {
            return filesByName;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::uintmax_t, std::string>>) {
            return filesBySizeAndName;
        }
        else {
            static_assert(AlwaysFalse<TKey>::value, "Unsupported key type");
        }
    }

    template<typename>
    struct AlwaysFalse : std::false_type {};
};
