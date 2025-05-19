#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ranges>
#include <mutex>

// GroupHandler tracks equivalence and distinction relationships among elements and assigns group IDs accordingly.
// This class allows users to input pairs of elements labeled as either "same" (belonging to the same group)
// or "different" (belonging to separate groups). Based on this information, the class dynamically assigns
// group IDs such that equal elements share the same ID, while differing elements are placed into distinct groups.
//
// The core functionality also includes a query method that determines whether a comparison between two elements
// is necessary, or if their relationship (same or different group) can be inferred from previous inputs.
//
// Useful for optimization scenarios where redundant comparisons can be skipped based on known group affiliations.
template<typename TValue>
class GroupHandler {
    std::unordered_map<TValue, int> groups;
    std::unordered_multimap<int, TValue> reverseGroups;
    std::unordered_map<TValue, std::unordered_set<int>> negativeGroups;
    std::unordered_map<int, std::vector<TValue>> grouped;
    int groupId{ 0 };
    std::mutex mtx;
public:
    void addSame(const TValue& a, const TValue& b) {
        std::lock_guard lock(mtx);
        auto itA = groups.find(a);
        auto itB = groups.find(b);

        if (itA != groups.end() && itB != groups.end()) {
            auto range = reverseGroups.equal_range(itB->second);
            for (auto itRG = range.first; itRG != range.second; ) {
                groups[itRG->second] = itA->second;

                reverseGroups.insert({ itA->second, itRG->second });
                itRG = reverseGroups.erase(itRG);
            }

            for (auto& [key, negSet] : negativeGroups) {
                if (negSet.erase(itB->second) > 0) {
                    negSet.insert(itA->second);
                }
            }
        }
        else if (itA != groups.end()) {
            groups[b] = itA->second;
            reverseGroups.insert({ itA->second, b });
        }
        else if (itB != groups.end()) {
            groups[a] = itB->second;
            reverseGroups.insert({ itB->second, a });
        }
        else {
            reverseGroups.insert({ groupId, a });
            reverseGroups.insert({ groupId, b });
            groups[a] = groups[b] = groupId++;
        }
    }

    void addDifferent(const TValue& a, const TValue& b) {
        std::lock_guard lock(mtx);
        if (groups.find(a) == groups.end()) {
            reverseGroups.insert({ groupId, a });
            groups[a] = groupId++;
        }

        if (groups.find(b) == groups.end()) {
            reverseGroups.insert({ groupId, b });
            groups[b] = groupId++;
        }
        negativeGroups[a].insert(groups[b]);
        negativeGroups[b].insert(groups[a]);
    }

    bool shouldItProcess(const TValue& a, const TValue& b) {
        std::lock_guard lock(mtx);
        auto itA = groups.find(a);
        auto itB = groups.find(b);
        if (itA != groups.end() && itB != groups.end()) {
            if (itA->second == itB->second) {
                return false;
            }

            auto itNegA = negativeGroups.find(a);
            if (itNegA != negativeGroups.end() && itNegA->second.find(itB->second) != itNegA->second.end()) {
                return false;
            }

            auto itNegB = negativeGroups.find(b);
            if (itNegB != negativeGroups.end() && itNegB->second.find(itA->second) != itNegB->second.end()) {
                return false;
            }

            return true;
        }
        return true;
    }

    auto buildGroupedList() {
        std::lock_guard lock(mtx);
        grouped.clear();
        for (const auto& [item, group] : groups) {
            grouped[group].push_back(item);
        }

        return grouped | std::views::filter([](const auto& pair) {
            return pair.second.size() > 1;
            });
    }
};
