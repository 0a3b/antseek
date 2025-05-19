#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cctype>
#include <filesystem>

namespace StringUtils {

    inline std::string pathToString(const std::filesystem::path& path) {
        std::u8string p = path.u8string();
        return std::string(reinterpret_cast<const char*>(p.data()), p.size());
    }

    inline std::vector<uint8_t> hexStringToBytes(std::string hex) {
        if (hex.size() >= 2 && (hex[0] == '0') && (hex[1] == 'x' || hex[1] == 'X')) {
            hex.erase(0, 2);
        }
        else if (!hex.empty() && (hex.back() == 'h' || hex.back() == 'H')) {
            hex.pop_back();
        }

        if (hex.empty() || hex.length() % 2 != 0) {
            throw std::invalid_argument("Hex string must have an even number of digits and not be empty.");
        }

        std::vector<uint8_t> bytes;
        bytes.reserve(hex.length() / 2);

        for (size_t i = 0; i < hex.length(); i += 2) {
            const char high = hex[i];
            const char low = hex[i + 1];
            if (!std::isxdigit(high) || !std::isxdigit(low)) {
                throw std::invalid_argument("Invalid hex character in: " + hex);
            }
            bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        }

        return bytes;
    }

    inline size_t parseSizeString(const std::string& input) {
        std::string numberStr = input;
        numberStr.erase(std::remove_if(numberStr.begin(), numberStr.end(), ::isspace), numberStr.end());

        if (numberStr.empty()) {
            throw std::invalid_argument("Empty size string");
        }

        size_t multiplier = 1;
        bool isHex = false;

        char lastChar = std::toupper(numberStr.back());
        if (std::isalpha(lastChar)) {
            switch (lastChar) {
            case 'K': multiplier = 1024ULL; break;
            case 'M': multiplier = 1024ULL * 1024; break;
            case 'G': multiplier = 1024ULL * 1024 * 1024; break;
            case 'T': multiplier = 1024ULL * 1024 * 1024 * 1024; break;
            case 'H': isHex = true; break; // Hex
            default:
                throw std::invalid_argument("Unknown size suffix: " + std::string(1, lastChar));
            }
            numberStr.pop_back();
        }

        size_t value = 0;

        if (numberStr.starts_with("0x") || numberStr.starts_with("0X")) {
            isHex = true;
            numberStr.erase(0, 2);
        }

        try {
            if (isHex) {
                value = std::stoull(numberStr, nullptr, 16);
            }
            else {
                value = std::stoull(numberStr);
            }
        }
        catch (const std::out_of_range&) {
            throw std::invalid_argument("Size value out of range: " + input);
        }
        catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid size value: " + input);
        }

        return value * multiplier;
    }

}
