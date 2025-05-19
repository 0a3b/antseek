#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

#include "HashUtils.hpp"

// PairQueue provides a mechanism to collect key/value pairs and generate all possible
// pairwise combinations of values that share the same key.
template<typename TValue>
class PairQueue {
private:
    std::vector<TValue> files;
    std::unordered_multimap<std::uintmax_t, TValue> filesBySize;
    std::unordered_multimap<std::string, TValue> filesByName;
    std::unordered_multimap<std::pair<std::uintmax_t, std::string>, TValue, HashUtils::pairHash> filesBySizeAndName;

    std::unordered_multimap<uint64_t, TValue> filesByHash;
    std::unordered_multimap<std::pair<std::uintmax_t, uint64_t>, TValue, HashUtils::pairHash> filesBySizeAndHash;
    std::unordered_multimap<std::pair<std::string, uint64_t>, TValue, HashUtils::pairHash> filesByNameAndHash;
    std::unordered_multimap<std::tuple<std::uintmax_t, std::string, uint64_t>, TValue, HashUtils::tupleHash> filesBySizeAndNameAndHash;

    std::deque<std::pair<TValue, TValue>> pairQueue;
    std::unordered_set<TValue> busyMainElements;

    std::unordered_map<int, std::vector<TValue>> grouped;
    std::unordered_map<std::uintmax_t, int> groupsBySize;
    std::unordered_map<std::string, int> groupsByName;
    std::unordered_map<std::pair<std::uintmax_t, std::string>, int, HashUtils::pairHash> groupsBySizeAndName;
    std::unordered_map<uint64_t, int> groupsByHash;
    std::unordered_map<std::pair<std::uintmax_t, uint64_t>, int, HashUtils::pairHash> groupsBySizeAndHash;
    std::unordered_map<std::pair<std::string, uint64_t>, int, HashUtils::pairHash> groupsByNameAndHash;
    std::unordered_map<std::tuple<std::uintmax_t, std::string, uint64_t>, int, HashUtils::tupleHash> groupsBySizeAndNameAndHash;

    std::mutex mtx;
    std::condition_variable_any cv;
    bool finished{ false }; // Indicates that no more elements will be added, but some elements may still be processing
    bool busy{ false }; // Indicates that each element pair in the queue has at least one member currently under processing.

public:
    template<typename TKey>
    void push(TKey key, const TValue& value, bool justCollect = false) {
        {
            std::lock_guard lock(mtx);
            auto& map = getMap<TKey>();

            if (!justCollect) {
                auto range = map.equal_range(key);
                for (auto it = range.first; it != range.second; ++it) {
                    pairQueue.push_back({ value, it->second });
                }
            }

            map.insert({ key, value });

            busy = false;
        }
        cv.notify_one();
    }

    void pushPassthrough(const TValue& value) {
        {
            std::lock_guard lock(mtx);

            for (auto& e : files) {
                pairQueue.push_back({ value, e });
            }

            files.push_back(value);
            busy = false;
        }
        cv.notify_one();
    }

    bool pop(std::pair<TValue, TValue>& out, std::stop_token stopToken) {
        std::unique_lock lock(mtx);

        while (true) {
            cv.wait(lock, stopToken, [this] { return (!busy && (!pairQueue.empty() || finished)); });

            if (pairQueue.empty() || stopToken.stop_requested()) {
                return false;
            }

            for (auto it = pairQueue.begin(); it != pairQueue.end(); ++it) {
                const auto& [a, b] = *it;
                if (busyMainElements.count(a) == 0 && busyMainElements.count(b) == 0) {
                    out = *it;
                    busyMainElements.insert(a);
                    pairQueue.erase(it);
                    return true;
                }
            }
            busy = true;
        }
    }

    void setProcessed(const std::pair<TValue, TValue>& task) {
        {
            std::lock_guard lock(mtx);
            busyMainElements.erase(task.first);
            busy = false;
        }
        cv.notify_all();
    }

    void setFinished() {
        {
            std::lock_guard lock(mtx);
            finished = true;
        }
        cv.notify_all();
    }

    template<typename TKey>
    auto buildGroupedList() {
        std::lock_guard lock(mtx);

        auto& map = getMap<TKey>();
        auto& groupMap = getGroupMap<TKey>();

        groupMap.clear();
        grouped.clear();
        int groupId = 0;
        int group;

        for (const auto& [key, value] : map) {
            if (groupMap.find(key) == groupMap.end()) {
                group = groupMap[key] = groupId++;
            }
            else {
                group = groupMap[key];
            }
            grouped[group].push_back(value);
        }

        return grouped;
    }

private:
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
        else if constexpr (std::is_same_v<TKey, uint64_t>) {
            return filesByHash;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::uintmax_t, uint64_t>>) {
            return filesBySizeAndHash;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::string, uint64_t>>) {
            return filesByNameAndHash;
        }
        else if constexpr (std::is_same_v<TKey, std::tuple<std::uintmax_t, std::string, uint64_t>>) {
            return filesBySizeAndNameAndHash;
        }
        else {
            static_assert(AlwaysFalse<TKey>::value, "Unsupported key type");
        }
    }

    template<typename TKey>
    auto& getGroupMap() {
        if constexpr (std::is_same_v<TKey, std::uintmax_t>) {
            return groupsBySize;
        }
        else if constexpr (std::is_same_v<TKey, std::string>) {
            return groupsByName;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::uintmax_t, std::string>>) {
            return groupsBySizeAndName;
        }
        else if constexpr (std::is_same_v<TKey, uint64_t>) {
            return groupsByHash;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::uintmax_t, uint64_t>>) {
            return groupsBySizeAndHash;
        }
        else if constexpr (std::is_same_v<TKey, std::pair<std::string, uint64_t>>) {
            return groupsByNameAndHash;
        }
        else if constexpr (std::is_same_v<TKey, std::tuple<std::uintmax_t, std::string, uint64_t>>) {
            return groupsBySizeAndNameAndHash;
        }
        else {
            static_assert(AlwaysFalse<TKey>::value, "Unsupported key type");
        }
    }

    template<typename>
    struct AlwaysFalse : std::false_type {};
};