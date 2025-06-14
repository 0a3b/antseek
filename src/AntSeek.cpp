#include "AntSeek.hpp"

#include <iostream>
#include <mutex>
#include <ranges>

#include "LoggingUtils.hpp"
#include "RegexUtils.hpp"
#include "HashUtils.hpp"
#include "CompareUtils.hpp"
#include "StringUtils.hpp"

namespace fs = std::filesystem;

void AntSeek::Config::setDirectories(const std::vector<std::string>& strvecDirectories) {
    directories.clear();
    for (const auto& str : strvecDirectories) {
        directories.emplace_back(str);
    }
}

void AntSeek::Config::setFilenamePatterns(const std::vector<std::string>& strvecFilenamePatterns) {
    filenamePatterns.clear();
    for (const auto& str : strvecFilenamePatterns) {
        try {
            filenamePatterns.emplace_back(str);
        }
        catch (const std::regex_error&) {
            throw std::runtime_error("Invalid regex pattern: " + str);
        }
    }
}

AntSeek::AntSeek(const Config& cfg) : config(cfg) {}

void AntSeek::start(const ThreadConfig& thrCfg) {
    dirQueue = std::make_unique<TreeQueue<fs::path>>(thrCfg.fileCollectorCount);

    for (const auto& d : config.directories) {
        if (!fs::exists(d)) {
            std::cerr << "Directory does not exist: " << d << "\n";
            continue;
        }
        else if (!fs::is_directory(d)) {
            std::cerr << "Not a directory: " << d << "\n";
            continue;
        }
        dirQueue->push(d);
    }

    activeFileCollectorCount.store(thrCfg.fileCollectorCount);
    for (auto i = thrCfg.fileCollectorCount; i; --i) {
        workers.emplace_back([this](std::stop_token st) {
            this->fileCollectorThread(st);
            }, stopSource.get_token());
    }

    if (config.operationMode == Config::OperationMode::ListFiles) {
        // No other threads needed
    }
    else if (config.operationMode == Config::OperationMode::AllVsAll) {
        activeHashCalculatorCount.store(thrCfg.hashCalculatorCount);
        for (auto i = thrCfg.hashCalculatorCount; i; --i) {
            workers.emplace_back([this](std::stop_token st) {
                this->hashCalculatorThread(st);
                }, stopSource.get_token());
        }

        if (config.matchContent != Config::MatchContent::None) {
            activeComparerCount.store(thrCfg.comparerCount);
            for (auto i = thrCfg.comparerCount; i; --i) {
                workers.emplace_back([this](std::stop_token st) {
                    this->compareContentThread(st);
                    }, stopSource.get_token());
            }
        }
    }
    else if (config.operationMode == Config::OperationMode::CompareToFile) {
        loadCompareToFile();

        if (config.hashMode != Config::HashMode::None) {
            referenceFileHash = HashUtils::hashFromFileChunk(fs::directory_entry(config.compareToFile), config.hashSize, config.hashMode == Config::HashMode::First);
        }

        activeComparerCount.store(thrCfg.comparerCount);
        for (auto i = thrCfg.comparerCount; i; --i) {
            workers.emplace_back([this](std::stop_token st) {
                this->compareContentFlexibleThread(st);
                }, stopSource.get_token());
        }
    }
    else {
        throw std::runtime_error("Unknown operation mode");
    }
}

void AntSeek::requestStop() {
    stopSource.request_stop();
}

void AntSeek::waitForFinish() {
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void AntSeek::getStatus() {
    throw std::logic_error("Not implemented");
}

void AntSeek::printResults() {
    waitForFinish();

    if (config.operationMode == Config::OperationMode::ListFiles ||
        config.operationMode == Config::OperationMode::CompareToFile)
    {
        for (const auto& p : results) {
            std::cout << StringUtils::pathToString(p) << "\n";
        }
    }
    else if (config.operationMode == Config::OperationMode::AllVsAll) {
        if (config.matchContent != Config::MatchContent::None) {
            auto it = groupHandler.buildGroupedList();
            for (const auto& [groupId, group] : it) {
                printGroup(groupId);
                for (const auto& file : group) {
                    printLine(groupId, StringUtils::pathToString(file));
                }
            }
        }
        else {
            auto it = getPairQueueResult() | std::views::filter([](const auto& pair) {
                return pair.second.size() > 1;
                });
            for (const auto& [groupId, group] : it) {
                printGroup(groupId);
                for (const auto& file : group) {
                    printLine(groupId, StringUtils::pathToString(file));
                }
            }
        }
    }
    else {
        throw std::runtime_error("Unknown operation mode");
    }
}

void AntSeek::loadCompareToFile() {
    referenceFileName = StringUtils::pathToString(config.compareToFile.filename());

    std::ifstream file(config.compareToFile, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error(std::string("Failed to open reference file: ") + referenceFileName);

    referenceFileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    referenceData.resize(referenceFileSize);
    if (!file.read(reinterpret_cast<char*>(referenceData.data()), referenceFileSize))
        throw std::runtime_error(std::string("Failed to read reference file: ") + referenceFileName);

    referenceDataMask = CompareUtils::generatePatternMask(referenceData, config.jokerBytes);
}

void AntSeek::printGroup(int groupId) {
    switch (config.outputFormat) {
        case Config::OutputFormat::Grouped:
            std::cout << "Group ID: " << groupId << "\n";
            break;
        case Config::OutputFormat::TSV:
        case Config::OutputFormat::Pipe:
            // No output
            break;
        default:
            throw std::runtime_error("Unknown output format");
    }
}

void AntSeek::printLine(int groupId, const std::string& line) {
    switch (config.outputFormat) {
        case Config::OutputFormat::Grouped:
            std::cout << "  " << line << "\n";
            break;
        case Config::OutputFormat::TSV:
            std::cout << groupId << "\t" << line << "\n";
            break;
        case Config::OutputFormat::Pipe:
            std::cout << groupId << "|" << line << "\n";
            break;
        default:
            throw std::runtime_error("Unknown output format");
    }
}

auto AntSeek::getPairQueueResult() -> std::unordered_map<int, std::vector<fs::path>> {
    if (config.hashMode == Config::HashMode::None) {
        if (config.matchFilename) {
            if (config.matchSize) {
                return hashQueue.buildGroupedList<std::pair<std::uintmax_t, std::string>>();
            }
            else {
                return hashQueue.buildGroupedList<std::string>();
            }
        }
        else if (config.matchSize) {
            return hashQueue.buildGroupedList<std::uintmax_t>();
        }
        throw std::runtime_error("Internal logic error - So long, and thanks for all the bugs.");
    }
    else {
        if (config.matchFilename) {
            if (config.matchSize) {
                return hashQueue.buildGroupedList<std::tuple<std::uintmax_t, std::string, uint64_t>>();
            }
            else {
                return hashQueue.buildGroupedList<std::pair<std::string, uint64_t>>();
            }
        }
        else if (config.matchSize) {
            return hashQueue.buildGroupedList<std::pair<std::uintmax_t, uint64_t>>();
        }
        return hashQueue.buildGroupedList<uint64_t>();
    }
}

void AntSeek::fileCollectorThread(std::stop_token st) {
    fs::path current;

    while (dirQueue->pop(current, st)) {
        try {
            for (const auto& entry : fs::directory_iterator(current)) {
                if (st.stop_requested()) return;

                if (entry.is_directory()) {
                    dirQueue->push(entry.path());
                }
                else if (entry.is_regular_file()) {
                    auto fn = StringUtils::pathToString(entry.path().filename());
                    if (RegexUtils::matchesAnyPattern(fn, config.filenamePatterns)) {

                        switch (config.operationMode) {
                            case Config::OperationMode::ListFiles:
                            {
                                std::lock_guard lock(results_mtx);
                                results.push_back(entry.path());
                            }
                            break;
                        case Config::OperationMode::CompareToFile:
                            {
                                auto fileSize = entry.file_size();
                                if ((referenceFileSize <= fileSize) &&
                                    (config.matchContent != Config::MatchContent::Full || fileSize == referenceFileSize) &&
                                    (!config.matchSize || fileSize == referenceFileSize) &&
                                    (!config.matchFilename || fn == referenceFileName) &&
                                    (config.hashMode == Config::HashMode::None ||
                                        referenceFileHash == HashUtils::hashFromFileChunk(entry, config.hashSize, config.hashMode == Config::HashMode::First)))
                                {
                                    fileQueue.pushPassthrough(entry);
                                }
                            }
                            break;
                        case Config::OperationMode::AllVsAll:
                            if (config.matchFilename) {
                                if (config.matchSize) {
                                    fileQueue.push(std::make_pair(entry.file_size(), fn), entry);
                                }
                                else {
                                    fileQueue.push(fn, entry);
                                }
                            }
                            else if (config.matchSize) {
                                fileQueue.push(entry.file_size(), entry);
                            }
                            else {
                                fileQueue.pushPassthrough(entry);
                            }
                            break;
                        default:
                            throw std::runtime_error("Unknown operation mode");
                        }
                    }
                }
            }
        }
        catch (const std::exception& e) {
            // TODO: skip?, log?
            LoggingUtils::writeToStderr(std::string("[ERROR] fileCollectorThread exception: ") + e.what() + "\n" +
                std::string("[ERROR] fileCollectorThread path: ") + current.string());
        }
    }

    if (activeFileCollectorCount.fetch_sub(1) == 1) {
        fileQueue.setFinished();
    }
}

void AntSeek::hashCalculatorThread(std::stop_token st) {
    fs::directory_entry current;
    bool justCollect = (config.matchContent == Config::MatchContent::None);

    while (fileQueue.pop(current, st)) {
        if (st.stop_requested()) return;

        if (config.hashMode == Config::HashMode::None) {
            if (config.matchFilename) {
                auto fn = StringUtils::pathToString(current.path().filename());
                if (config.matchSize) {
                    hashQueue.push(std::make_pair(current.file_size(), fn), current.path(), justCollect);
                }
                else {
                    hashQueue.push(fn, current.path(), justCollect);
                }
            }
            else if (config.matchSize) {
                hashQueue.push(current.file_size(), current.path(), justCollect);
            }
            else {
                hashQueue.pushPassthrough(current.path());
            }
        }
        else {
            uint64_t hash = HashUtils::hashFromFileChunk(current, config.hashSize, config.hashMode == Config::HashMode::First);
            if (config.matchFilename) {
                auto fn = StringUtils::pathToString(current.path().filename());
                if (config.matchSize) {
                    hashQueue.push(std::make_tuple(current.file_size(), fn, hash), current.path(), justCollect);
                }
                else {
                    hashQueue.push(std::make_pair(fn, hash), current.path(), justCollect);
                }
            }
            else if (config.matchSize) {
                hashQueue.push(std::make_pair(current.file_size(), hash), current.path(), justCollect);
            }
            else {
                hashQueue.push(hash, current.path(), justCollect);
            }
        }
    }

    if (activeHashCalculatorCount.fetch_sub(1) == 1) {
        hashQueue.setFinished();
    }
}

void AntSeek::compareContentThread(std::stop_token st) {
    std::pair<fs::path, fs::path> current;

    while (hashQueue.pop(current, st)) {
        if (st.stop_requested()) return;

        if (groupHandler.shouldItProcess(current.first, current.second)) {
            switch (CompareUtils::compareFileContents(current.first, current.second)) {
            case CompareUtils::MatchResult::Match:
                groupHandler.addSame(current.first, current.second);
                break;
            case CompareUtils::MatchResult::NoMatch:
                groupHandler.addDifferent(current.first, current.second);
                break;
            case CompareUtils::MatchResult::Error:
                LoggingUtils::writeToStderr("[ERROR] Error comparing files: " + current.first.string() + " and " + current.second.string());
                break;
            }
        }
        hashQueue.setProcessed(current);
    }

    if (activeComparerCount.fetch_sub(1) == 1) {
        std::cout << "All threads finished processing.\n";
    }
}

void AntSeek::compareContentFlexibleThread(std::stop_token st) {
    fs::directory_entry current;

    while (fileQueue.pop(current, st)) {
        if (st.stop_requested()) return;

        CompareUtils::MatchResult res;
        switch (config.matchContent) {
            case Config::MatchContent::Begin:
            case Config::MatchContent::Full:
                res = CompareUtils::compareFileContentsFlexible(current, referenceData, referenceDataMask, false);
                break;
            case Config::MatchContent::End:
                res = CompareUtils::compareFileContentsFlexible(current, referenceData, referenceDataMask, true);
                break;
            case Config::MatchContent::Find:
                res = CompareUtils::searchInFileContentsFlexible(current, referenceData, referenceDataMask);
                break;
        }

        if (res == CompareUtils::MatchResult::Match) {
            results.push_back(current);
        }
    }

    if (activeComparerCount.fetch_sub(1) == 1) {
        std::cout << "All threads finished processing.\n";
    }
}
