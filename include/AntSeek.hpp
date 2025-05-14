#pragma once

#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stop_token>
#include <queue>
#include <vector>
#include <unordered_map>
#include <ranges>
#include <string>
#include <regex>

#include "LoggingUtils.hpp"
#include "RegexUtils.hpp"
#include "HashUtils.hpp"
#include "CompareUtils.hpp"
#include "StringUtils.hpp"

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

        void setDirectories(const std::vector<std::string>& strvecDirectories) {
            directories.clear();
            for (const auto& str : strvecDirectories) {
                directories.emplace_back(str);
            }
        }

        void setFilenamePatterns(const std::vector<std::string>& strvecFilenamePatterns) {
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
    };

	struct ThreadConfig {
		int fileCollectorCount{ 4 };
		int hashCalculatorCount{ 4 };
		int comparerCount{ 4 };
		size_t bufferSize{ 8192 };
	};

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

    std::vector<fs::path> results;  // vagy B
    std::mutex results_mtx;
public:
    explicit AntSeek(const Config& cfg) : config(cfg) {}

    void start(const ThreadConfig& thrCfg) {
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
        for (auto i = thrCfg.fileCollectorCount ; i ; --i) {
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
            throw std::logic_error("Not implemented");
		}
		else {
            throw std::runtime_error("Unknown operation mode");
        }     
    }

    void requestStop() {
        stopSource.request_stop();
    }

    void waitForFinish() {
        for (auto& worker : workers) {
			if (worker.joinable()) {
                worker.join();
			}
        }
    }

	void getStatus() {
        throw std::logic_error("Not implemented");
    }

    void printResults() {
        waitForFinish();

        if (config.operationMode == Config::OperationMode::ListFiles) {
            for (const auto& p : results) {
                std::cout << StringUtils::pathToString(p) << "\n";
            }
        }
        else if (config.operationMode == Config::OperationMode::AllVsAll) {               
            if (config.matchContent != Config::MatchContent::None) {
                auto it = groupHandler.buildGroupedList();
                for (const auto& [groupId, group] : it) {
                    std::cout << "Group ID: " << groupId << "\n";
                    for (const auto& file : group) {
                        std::cout << "  " << StringUtils::pathToString(file) << "\n";
                    }
                }
            }
            else {
                auto it = getPairQueueResult() | std::views::filter([](const auto& pair) {
                    return pair.second.size() > 1;
                    });
                for (const auto& [groupId, group] : it) {
                    std::cout << "Group ID: " << groupId << "\n";
                    for (const auto& file : group) {
                        std::cout << "  " << StringUtils::pathToString(file) << "\n";
                    }
                }            
            }
        }
        else if (config.operationMode == Config::OperationMode::CompareToFile) {
            throw std::logic_error("Not implemented");
        }
        else {
            throw std::runtime_error("Unknown operation mode");
        }
    }

private:
    auto getPairQueueResult() -> std::unordered_map<int, std::vector<fs::path>> {
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

    void fileCollectorThread(std::stop_token st) {
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
                                    throw std::logic_error("Not implemented");
								//	fileCompareToOneQueue.push(entry);
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

    void hashCalculatorThread(std::stop_token st) {
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

    void compareContentThread(std::stop_token st) {
        std::pair<fs::path, fs::path> current;

        while (hashQueue.pop(current, st)) {
            if (st.stop_requested()) return;

            if (groupHandler.shouldItProcess(current.first, current.second)) {
                switch (CompareUtils::compareFileContents(current.first, current.second)) {
				    case CompareUtils::FileCompareResult::Equal:
					    groupHandler.addSame(current.first, current.second);
					    break;
				    case CompareUtils::FileCompareResult::NotEqual:
                        groupHandler.addDifferent(current.first, current.second);
                        break;
				    case CompareUtils::FileCompareResult::Error:
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

};