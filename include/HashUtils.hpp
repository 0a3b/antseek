#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
#include "xxhash.h"

namespace HashUtils {

    constexpr std::size_t goldenRatio =
        (sizeof(std::size_t) == 8)
        ? static_cast<std::size_t>(0x9e3779b97f4a7c15ull)   // 64-bit
        : static_cast<std::size_t>(0x9e3779b9ul);           // 32-bit

    template <typename TValue>
    inline void hashCombine(std::size_t& seed, const TValue& val)
    {
        seed ^= std::hash<TValue>{}(val)+goldenRatio + (seed << 6) + (seed >> 2);
    }

    struct pairHash {
        template<typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2>& p) const {
            std::size_t seed = 0;
            hashCombine(seed, p.first);
            hashCombine(seed, p.second);
            return seed;
        }
    };

    struct tupleHash
    {
        template <typename... TValue>
        std::size_t operator()(const std::tuple<TValue...>& tpl) const
        {
            std::size_t seed = 0;
            std::apply([&seed](const auto&... args) {
                (hashCombine(seed, args), ...);
                }, tpl);
            return seed;
        }
    };

    struct directoryEntryHash {
        size_t operator()(const std::filesystem::directory_entry& entry) const noexcept {
            return std::hash<std::filesystem::path>{}(entry.path());
        }
    };

    inline uint64_t hashFromFileChunk(const std::filesystem::directory_entry& entry, std::size_t byteCount, bool fromStart = true)
    {
        if (!entry.is_regular_file())
            throw std::invalid_argument("Not a regular file.");

        std::ifstream file(entry.path(), std::ios::binary);
        if (!file)
            throw std::runtime_error("Failed to open file.");

        const std::uintmax_t file_size = entry.file_size();
        if (file_size < byteCount)
            byteCount = static_cast<std::int32_t>(file_size);

        if (!fromStart) {
            file.seekg(-static_cast<std::streamoff>(byteCount), std::ios::end);
        }

        std::vector<uint8_t> buffer(byteCount);
        file.read(reinterpret_cast<char*>(buffer.data()), byteCount);
        if (!file)
            throw std::runtime_error("Failed to read file content.");

        return XXH3_64bits(buffer.data(), byteCount);
    }

}