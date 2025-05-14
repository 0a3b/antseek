#pragma once

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include <algorithm>

namespace CompareUtils {

    namespace fs = std::filesystem;

    enum class FileCompareResult {
        Equal,
        NotEqual,
        Error
    };

    inline FileCompareResult compareFileContents(const fs::directory_entry& file1, const fs::directory_entry& file2, std::size_t buffer_size = 8192) {
        try {
            if (!file1.is_regular_file() || !file2.is_regular_file())
                return FileCompareResult::Error;

            if (file1.file_size() != file2.file_size())
                return FileCompareResult::NotEqual;

            std::ifstream f1(file1.path(), std::ios::binary);
            std::ifstream f2(file2.path(), std::ios::binary);

            if (!f1.is_open() || !f2.is_open())
                return FileCompareResult::Error;

            std::vector<char> buffer1(buffer_size);
            std::vector<char> buffer2(buffer_size);

            while (f1 && f2) {
                f1.read(buffer1.data(), buffer_size);
                f2.read(buffer2.data(), buffer_size);

                std::streamsize bytesRead1 = f1.gcount();
                std::streamsize bytesRead2 = f2.gcount();

                if (bytesRead1 != bytesRead2)
                    return FileCompareResult::Error; // Possible I/O error or background modification?

                if (!std::equal(buffer1.begin(), buffer1.begin() + bytesRead1, buffer2.begin()))
                    return FileCompareResult::NotEqual;
            }

            return FileCompareResult::Equal;
        }
        catch (const std::exception&) {
            return FileCompareResult::Error;
        }
    }

    inline FileCompareResult compareFileContents(const fs::path& file1, const fs::path& file2, std::size_t buffer_size = 8192) {
        try {
            std::ifstream f1(file1, std::ios::binary);
            std::ifstream f2(file2, std::ios::binary);

            if (!f1.is_open() || !f2.is_open())
                return FileCompareResult::Error;

            std::vector<char> buffer1(buffer_size);
            std::vector<char> buffer2(buffer_size);

            while (f1 && f2) {
                f1.read(buffer1.data(), buffer_size);
                f2.read(buffer2.data(), buffer_size);

                std::streamsize bytesRead1 = f1.gcount();
                std::streamsize bytesRead2 = f2.gcount();

                if (bytesRead1 != bytesRead2)
                    return FileCompareResult::Error; // Possible I/O error or background modification?

                if (!std::equal(buffer1.begin(), buffer1.begin() + bytesRead1, buffer2.begin()))
                    return FileCompareResult::NotEqual;
            }

            return FileCompareResult::Equal;
        }
        catch (const std::exception&) {
            return FileCompareResult::Error;
        }
    }

}
