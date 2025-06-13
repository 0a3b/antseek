#pragma once

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include <span>
#include <algorithm>
#include <cstring>

namespace CompareUtils {

    namespace fs = std::filesystem;

    enum class MatchResult {
        Match,
        NoMatch,
        Error
    };

    inline MatchResult compareFileContents(const fs::directory_entry& file1, const fs::directory_entry& file2, std::size_t buffer_size = 8192) {
        try {
            if (!file1.is_regular_file() || !file2.is_regular_file())
                return MatchResult::Error;

            if (file1.file_size() != file2.file_size())
                return MatchResult::NoMatch;

            std::ifstream f1(file1.path(), std::ios::binary);
            std::ifstream f2(file2.path(), std::ios::binary);

            if (!f1.is_open() || !f2.is_open())
                return MatchResult::Error;

            std::vector<char> buffer1(buffer_size);
            std::vector<char> buffer2(buffer_size);

            while (f1 && f2) {
                f1.read(buffer1.data(), buffer_size);
                f2.read(buffer2.data(), buffer_size);

                std::streamsize bytesRead1 = f1.gcount();
                std::streamsize bytesRead2 = f2.gcount();

                if (bytesRead1 != bytesRead2)
                    return MatchResult::Error; // Possible I/O error or background modification?

                if (!std::equal(buffer1.begin(), buffer1.begin() + bytesRead1, buffer2.begin()))
                    return MatchResult::NoMatch;
            }

            return MatchResult::Match;
        }
        catch (const std::exception&) {
            return MatchResult::Error;
        }
    }

    inline MatchResult compareFileContents(const fs::path& file1, const fs::path& file2, std::size_t buffer_size = 8192) {
        try {
            std::ifstream f1(file1, std::ios::binary);
            std::ifstream f2(file2, std::ios::binary);

            if (!f1.is_open() || !f2.is_open())
                return MatchResult::Error;

            std::vector<char> buffer1(buffer_size);
            std::vector<char> buffer2(buffer_size);

            while (f1 && f2) {
                f1.read(buffer1.data(), buffer_size);
                f2.read(buffer2.data(), buffer_size);

                std::streamsize bytesRead1 = f1.gcount();
                std::streamsize bytesRead2 = f2.gcount();

                if (bytesRead1 != bytesRead2)
                    return MatchResult::Error; // Possible I/O error or background modification?

                if (!std::equal(buffer1.begin(), buffer1.begin() + bytesRead1, buffer2.begin()))
                    return MatchResult::NoMatch;
            }

            return MatchResult::Match;
        }
        catch (const std::exception&) {
            return MatchResult::Error;
        }
    }

    inline bool compareWithMask(const std::span<const uint8_t> data, const std::span<const uint8_t> reference, const std::span<const uint64_t> referenceMask) {
// TODO: SIMD optimalisations
            const auto refSize = reference.size();
            size_t blocks = (refSize + 63) >> 6;
            size_t bytePos = 0;        
            for (size_t b = 0 ; b < blocks ; ++b) {
                uint64_t rmb = referenceMask[b];
                
                if (rmb == 0) {
                    bytePos += 64;                   
                    continue;
                }
                
                if (rmb == ~0ULL) {
                    if (std::memcmp(reference.data() + bytePos, data.data() + bytePos, 64) != 0)
                        return false;

                    bytePos += 64;
                }
                else {
                    size_t bytesLeft = refSize - bytePos;
                    size_t cnt = bytesLeft < 64 ? bytesLeft : 64;
                    for (size_t i = 0  ; i < cnt ; ++i, ++bytePos) {
                        if (((rmb >> i) & 1) &&
                            (reference[bytePos] != data[bytePos]))
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
    }

    inline MatchResult compareFileContentsFlexible(const fs::path& file, const std::span<const uint8_t> reference, const std::span<const uint64_t> referenceMask, bool checkEnd = false) {
    // IMPORTANT: Bits in the last element of referenceMask that correspond to positions beyond the end of 'reference' must NOT be set.
        try {
            const auto refSize = reference.size();
            if (refSize == 0)
                return MatchResult::Match;

            if (referenceMask.size() < ((refSize + 63) >> 6))
                return MatchResult::Error;

            std::ifstream f(file, std::ios::binary);
            if (!f.is_open())
                return MatchResult::Error;

            f.seekg(0, std::ios::end);
            auto fileSize = f.tellg();
            if (fileSize == std::streampos(-1))
                return MatchResult::Error;

            if (fileSize < static_cast<decltype(fileSize)>(refSize))
                return MatchResult::NoMatch;

            f.seekg(checkEnd ? fileSize - static_cast<std::streamoff>(refSize) : std::streampos(0));
            if (!f)
                return MatchResult::Error;

            std::vector<uint8_t> buffer(refSize);
            f.read(reinterpret_cast<char*>(buffer.data()), refSize);

            if (f.gcount() != refSize || f.fail())
                    return MatchResult::Error;

            if (compareWithMask(buffer, reference, referenceMask))
                return MatchResult::Match;
                
            return MatchResult::NoMatch;
        }
        catch (const std::exception&) {
            return MatchResult::Error;
        }
    }

    inline bool searchWithMask(std::span<const uint8_t> data, const std::span<const uint8_t> reference, const std::span<const uint64_t> referenceMask, std::size_t size) {
// TODO: implement skip table
        if (size < reference.size())
            return false;

        size_t end = size - reference.size();
        
        for (size_t i = 0 ; i <= end ; ++i) {
            if (compareWithMask(data.subspan(i), reference, referenceMask))
                return true;
        }

        return false;
    }

    inline MatchResult searchInFileContentsFlexible(const fs::path& file, const std::span<const uint8_t> reference, const std::span<const uint64_t> referenceMask, std::size_t baseBufferSize = 8192) {
    // IMPORTANT: Bits in the last element of referenceMask that correspond to positions beyond the end of 'reference' must NOT be set.
        try {
            const auto refSize = reference.size();
            if (refSize == 0)
                return MatchResult::Match;

            if (referenceMask.size() < ((refSize + 63) >> 6))
                return MatchResult::Error;

            std::ifstream f(file, std::ios::binary);
            if (!f.is_open())
                return MatchResult::Error;

            size_t overlap = refSize - 1;
            std::vector<uint8_t> buffer(baseBufferSize + overlap);
            
            f.read(reinterpret_cast<char*>(buffer.data()), baseBufferSize + overlap);
            std::streamsize bytesRead = f.gcount();
            if (bytesRead < refSize)
                return MatchResult::NoMatch;

            if (searchWithMask(buffer, reference, referenceMask, bytesRead))
                return MatchResult::Match;
            
            while (f) {
                std::copy(buffer.end() - overlap, buffer.end(), buffer.begin());
                
                f.read(reinterpret_cast<char*>(buffer.data()) + overlap, baseBufferSize);
                bytesRead = f.gcount();
                if (bytesRead + overlap < refSize)
                    break;
                
                if (searchWithMask(buffer, reference, referenceMask, bytesRead + overlap))
                    return MatchResult::Match;
            }
            
            return MatchResult::NoMatch;
        }
        catch (const std::exception&) {
            return MatchResult::Error;
        }
    }

    inline std::vector<uint64_t> generatePatternMask(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern) {
        size_t dataSize = data.size();
        size_t patternSize = pattern.size();

        size_t maskSize = (dataSize + 63) >> 6;
        std::vector<uint64_t> mask(maskSize, ~0ULL);

        size_t leftover = dataSize & 63;
        if (leftover > 0) {
            mask.back() &= (1ULL << leftover) - 1;
        }

        if (patternSize > dataSize || patternSize == 0)
            return mask;

        size_t maskPos = 0;

        for (size_t pos = 0 ; pos + patternSize <= dataSize ; ++pos) {
            if (std::equal(pattern.begin(), pattern.end(), data.begin() + pos)) {
                size_t end = pos + patternSize;
                for (size_t p = std::max(maskPos, pos) ; p < end ; ++p) {
                    mask[p >> 6] &= ~(1ULL << (p & 63));
                }
                maskPos = end;
            }
        }

        return mask;
    }
}
