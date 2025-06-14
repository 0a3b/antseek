#pragma once

#include <string>
#include <vector>
#include <unordered_map>


class ArgParser {
public:
    explicit ArgParser(int argc, char* argv[]) {
        parse(argc, argv);
    }

    const std::vector<std::string>& getList(const std::string& key) const {
        static const std::vector<std::string> empty;
        auto it = args.find(key);
        return it != args.end() ? it->second : empty;
    }

    size_t getValueCount(const std::string& key) const {
        auto it = args.find(key);
        return it != args.end() ? it->second.size() : 0;
    }

    std::string get(const std::string& key, size_t index = 0) const {
        const auto& vals = getList(key);
        return (index < vals.size()) ? vals[index] : "";
    }

    bool has(const std::string& key) const {
        return args.contains(key);
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> args;

    void parse(int argc, char* argv[]) {
        std::string key;
        for (int i = 1; i < argc; ++i) {
            std::string token = argv[i];

            if (token.starts_with("--")) {
                key = token; // token.substr(2);
                args[key] = {};
            }
            else if (token.starts_with("-")) {
                key = token; // token.substr(1);
                args[key] = {};
            }
            else if (!key.empty()) {
                args[key].push_back(strip(token));
            }
        }
    }

    static std::string strip(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }
};

