#pragma once

#include <string>
#include <vector>
#include <regex>

namespace RegexUtils {

    inline bool matchesAnyPattern(const std::string& input, const std::vector<std::regex>& patterns) {
        for (const auto& p : patterns) {
            if (std::regex_match(input, p)) {
                return true;
            }
        }
        return false;
    }

}