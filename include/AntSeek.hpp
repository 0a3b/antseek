#pragma once

#include <filesystem>
#include <thread>
#include <atomic>
#include <stop_token>
#include <vector>
#include <unordered_map>
#include <string>
#include <regex>

#include "TreeQueue.hpp"
#include "FileQueue.hpp"
#include "PairQueue.hpp"
#include "GroupHandler.hpp"

namespace fs = std::filesystem;

class AntSeek {
public:
    struct Config {
        std::vector<std::regex> filenamePatterns;
        std::vector<fs::path> directories;
        fs::path compareToFile;
        bool matchFilename{ false };
        bool matchSize{ false };
        enum class MatchContent { None, Full, Begin, End, Find } matchContent{ MatchContent::None };
        enum class HashMode { None, First, Last } hashMode{ HashMode::None };
        size_t hashSize{ 4096 };
        std::vector<uint8_t> jokerBytes;
        enum class OperationMode { ListFiles, CompareToFile, AllVsAll } operationMode{ OperationMode::ListFiles };
        enum class OutputFormat { Grouped, TSV, Pipe } outputFormat{ OutputFormat::Pipe };

        void setDirectories(const std::vector<std::string>& strvecDirectories);
        void setFilenamePatterns(const std::vector<std::string>& strvecFilenamePatterns);
    };

    struct ThreadConfig {
        int fileCollectorCount{ 4 };
        int hashCalculatorCount{ 4 };
        int comparerCount{ 4 };
        size_t bufferSize{ 8192 };
    };

    explicit AntSeek(const Config& cfg);
    void start(const ThreadConfig& thrCfg);
    void requestStop();
    void waitForFinish();
    void getStatus();
    void printResults();

private:
    Config config;
    std::unique_ptr <TreeQueue<fs::path>> dirQueue;
    FileQueue<fs::directory_entry> fileQueue;
    PairQueue<fs::path> hashQueue;
    GroupHandler<fs::path> groupHandler;
    std::vector<std::jthread> workers;
    std::stop_source stopSource;

    std::atomic<int> activeFileCollectorCount{ 0 };
    std::atomic<int> activeHashCalculatorCount{ 0 };
    std::atomic<int> activeComparerCount{ 0 };

    std::uintmax_t referenceFileSize{ 0 };
    std::string referenceFileName;
    uint64_t referenceFileHash{ 0 };
    std::vector<uint8_t> referenceData;
    std::vector<uint64_t> referenceDataMask;

    std::vector<fs::path> results;
    std::mutex results_mtx;
    
    void loadCompareToFile();
    void printGroup(int groupId);
    void printLine(int groupId, const std::string& line);
    std::unordered_map<int, std::vector<fs::path>> getPairQueueResult();
    void fileCollectorThread(std::stop_token st);
    void hashCalculatorThread(std::stop_token st);
    void compareContentThread(std::stop_token st);
    void compareContentFlexibleThread(std::stop_token st);
};