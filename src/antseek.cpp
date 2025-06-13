#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <iostream>
#include <algorithm>

#include "version.hpp"
#include "ArgParser.hpp"
#include "AntSeek.hpp"
#include "StringUtils.hpp"

constexpr const char* ArgOpt_directories = "--directories";
constexpr const char* ArgOpt_filenames = "--filenames";
constexpr const char* ArgOpt_match_filenames = "--match-filenames";
constexpr const char* ArgOpt_match_size = "--match-size";
constexpr const char* ArgOpt_match_hash = "--match-hash";
constexpr const char* ArgOpt_compare_content = "--compare-content";
constexpr const char* ArgOpt_compare_to = "--compare-to";
constexpr const char* ArgOpt_set_joker = "--set-joker";
constexpr const char* ArgOpt_compare_everything = "--compare-everything";
constexpr const char* ArgOpt_output_format = "--output-format";
constexpr const char* ArgOpt_help = "--help";
constexpr const char* ArgOpt_version = "--version";

constexpr const char* ArgVal_match_hash_first = "first";
constexpr const char* ArgVal_match_hash_last = "last";

constexpr const char* ArgVal_compare_content_full = "full";
constexpr const char* ArgVal_compare_content_begin = "begin";
constexpr const char* ArgVal_compare_content_end = "end";
constexpr const char* ArgVal_compare_content_find = "find";

constexpr const char* ArgOpt_output_format_pipe = "pipe";
constexpr const char* ArgOpt_output_format_tsv = "tsv";
constexpr const char* ArgOpt_output_format_grouped = "grouped";

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    std::wcout.imbue(std::locale("en_US.UTF-8"));
#endif

    ArgParser args(argc, argv);

    if (args.has(ArgOpt_help)) {
        std::cout <<
            "Usage: antseek --directories <dir1> <dir2> ... --filenames <pattern1> <pattern2> ...\n"
            << ArgOpt_help << "                                     Show this help message\n"
            << ArgOpt_version << "                                  Show version information\n"
            << ArgOpt_output_format << " <pipe|tsv|grouped>         Output format (default: pipe)\n"
            << ArgOpt_directories << " <dir1> <dir2> ...            Directories to process\n"
            << ArgOpt_filenames << " <pattern1> <pattern2> ...      Filename patterns to match (expects C++ regex syntax)\n"
            << ArgOpt_match_filenames << "                          Match files based on their filenames\n"
            << ArgOpt_match_size << "                               Match files based on their size\n"
            << ArgOpt_match_hash << " <first|last> <size>           Compare files by hashing the first or last N bytes (default: 4k)\n"
            << ArgOpt_compare_content << " <full|begin|end|find>    Enables file comparison based on content.\n"
            "                                             - full: Compares the full content of each file.\n"
            "                                             - begin, end, find: Must be used together with the --compare-to option.\n"
            "                                               - begin: Checks if the specified file's content appears at the beginning of each target file.\n"
            "                                               - end: Checks if the specified file's content appears at the end of each target file.\n"
            "                                               - find: Searches for the specified file's content anywhere within each target file.\n"
            << ArgOpt_compare_to << " <file>                        Compare files based on the specified file's content.\n"
            << ArgOpt_set_joker << " <value>                        Hexadecimal joker value to ignore during comparison (e.g. 0x000000FF; high-order bytes first).\n"
            << ArgOpt_compare_everything << "                       Compare each file against every other file.\n"
            "\n"
            "When '" << ArgOpt_compare_everything << "' and '" << ArgOpt_compare_content << " " << ArgVal_compare_content_full <<
            "' is used, the program implicitly activates both '" << ArgOpt_match_size << "' and '" << ArgOpt_match_hash << " " << ArgVal_match_hash_first <<
            "' with a default hash block size of 4 KB.\n"
            "\n"
            "Typical Use Cases\n"
            "-----------------\n"
            "\n"
            "Scan and list all.txt files located in both c:\\temp and c:\\mystuff.\n"
#ifdef _WIN32
            "antseek --directories c:\\temp c:\\mystuff --filenames \".*\\.txt$\"\n"
#else
            "./antseek --directories ~/temp ~/mystuff --filenames \".*\\.txt$\"\n"
#endif
            "\n"
            "List all capture_[6-8 digits date].jpg and .jpeg files that have at least one duplicate (fast, approximate match, filesize and first 2KB hash will be checked)\n"
#ifdef _WIN32
            "antseek --directories c:\\temp --filenames \"^capture_\\d{6,8}\\.(jpg|jpeg)$\" --compare-everything --match-size --match-hash first 2K\n"
#else
            "./antseek --directories ~/temp --filenames \"^capture_\\d{6,8}\\.(jpg|jpeg)$\" --compare-everything --match-size --match-hash first 2K\n"
#endif
            "\n"
            "List all .exe or .src files in c:\\temp that have at least one duplicate (accurate but slower)\n"
#ifdef _WIN32
            "antseek --directories c:\\temp --filenames \".*\\.(exe|src)$\" --compare-everything --compare-content full\n"
#else
            "./antseek --directories ~/temp --filenames \".*\\.(exe|src)$\" --compare-everything --compare-content full\n"
#endif
            ;
        return 0;
    }

    if (args.has(ArgOpt_version)) {
        std::cout << "AntSeek version " << VERSION_STRING << "\n";
        return 0;
    }

    if (args.getValueCount(ArgOpt_directories) == 0) {
        std::cout << "Error: No " << ArgOpt_directories << " specified.\n";
        return 1;
    }

    if (args.getValueCount(ArgOpt_filenames) == 0) {
        std::cout << "Error: No " << ArgOpt_filenames << " specified.\n";
        return 1;
    }

    if (args.has(ArgOpt_compare_everything) && args.has(ArgOpt_compare_to)) {
        std::cout << "Error: Invalid combination of options: " << ArgOpt_compare_everything << " and " << ArgOpt_compare_to << " cannot be used together.\n";
        return 1;
    }

    if (args.has(ArgOpt_set_joker) && !args.has(ArgOpt_compare_to)) {
        std::cout << "Error: Invalid combination of options: " << ArgOpt_set_joker << " requires " << ArgOpt_compare_to << ".\n";
        return 1;
    }

    if (args.has(ArgOpt_match_filenames) && args.getValueCount(ArgOpt_match_filenames) > 0) {
        std::cout << "Error: The " << ArgOpt_match_filenames << " option does not accept any parameters.\n";
        return 1;
    }

    if (args.has(ArgOpt_match_size) && args.getValueCount(ArgOpt_match_size) > 0) {
        std::cout << "Error: The " << ArgOpt_match_size << " option does not accept any parameters.\n";
        return 1;
    }

    if (args.has(ArgOpt_compare_everything)) {
        if (!(args.has(ArgOpt_match_filenames) || args.has(ArgOpt_match_size) || args.has(ArgOpt_match_hash) || args.has(ArgOpt_compare_content))) {
            std::cout << "Error: The " << ArgOpt_compare_everything << " option requires at least one of the following options: "
                << ArgOpt_match_filenames << ", " << ArgOpt_match_size << ", " << ArgOpt_match_hash << ", or " << ArgOpt_compare_content << ".\n";
            return 1;
        }

        if (args.has(ArgOpt_compare_content) && args.get(ArgOpt_compare_content) != ArgVal_compare_content_full) {
            std::cout << "Error: The " << ArgOpt_compare_everything << " option can only be used with " << ArgOpt_compare_content << " if set to " << ArgVal_compare_content_full << ".\n";
            return 1;
        }
    }

    if (args.has(ArgOpt_compare_to) && !args.has(ArgOpt_compare_content)) {
        std::cout << "Error: The " << ArgOpt_compare_to << " option requires option " << ArgOpt_compare_content << ".\n";
        return 1;
    }

    AntSeek::Config config;
    config.setDirectories(args.getList(ArgOpt_directories));
    config.setFilenamePatterns(args.getList(ArgOpt_filenames));
    config.matchFilename = args.has(ArgOpt_match_filenames);
    config.matchSize = args.has(ArgOpt_match_size);

    if (args.has(ArgOpt_compare_everything)) {
        config.operationMode = AntSeek::Config::OperationMode::AllVsAll;
    }
    else if (args.has(ArgOpt_compare_to)) {
        config.operationMode = AntSeek::Config::OperationMode::CompareToFile;
    }
    else {
        config.operationMode = AntSeek::Config::OperationMode::ListFiles;
    }

    if (args.has(ArgOpt_compare_to)) {
        config.compareToFile = args.get(ArgOpt_compare_to);
    }

    if (args.has(ArgOpt_set_joker)) {
        config.jokerBytes = StringUtils::hexStringToBytes(args.get(ArgOpt_set_joker));
    }

    if (args.has(ArgOpt_compare_content)) {
        std::string content_mode = args.get(ArgOpt_compare_content);
        if (content_mode == ArgVal_compare_content_full) {
            config.matchContent = AntSeek::Config::MatchContent::Full;
        }
        else if (content_mode == ArgVal_compare_content_begin) {
            config.matchContent = AntSeek::Config::MatchContent::Begin;
        }
        else if (content_mode == ArgVal_compare_content_end) {
            config.matchContent = AntSeek::Config::MatchContent::End;
        }
        else if (content_mode == ArgVal_compare_content_find) {
            config.matchContent = AntSeek::Config::MatchContent::Find;
        }
        else {
            std::cout << "Error: Invalid value for " << ArgOpt_compare_content << ": " << content_mode << "\n";
            return 1;
        }
    }

    if (args.has(ArgOpt_match_hash)) {
        std::string hash_mode = args.get(ArgOpt_match_hash);
        if (hash_mode == ArgVal_match_hash_first) {
            config.hashMode = AntSeek::Config::HashMode::First;
        }
        else if (hash_mode == ArgVal_match_hash_last) {
            config.hashMode = AntSeek::Config::HashMode::Last;
        }
        else {
            std::cout << "Error: Invalid value for " << ArgOpt_match_hash << ": " << hash_mode << "\n";
            return 1;
        }

        if (args.getValueCount(ArgOpt_match_hash) > 1) {
            config.hashSize = StringUtils::parseSizeString(args.get(ArgOpt_match_hash, 1));
        }
    }

    if (args.has(ArgOpt_output_format)) {
        std::string format = args.get(ArgOpt_output_format);
        if (format == ArgOpt_output_format_pipe) {
            config.outputFormat = AntSeek::Config::OutputFormat::Pipe;
        } else if (format == ArgOpt_output_format_tsv) {
            config.outputFormat = AntSeek::Config::OutputFormat::TSV;
        } else if (format == ArgOpt_output_format_grouped) {
            config.outputFormat = AntSeek::Config::OutputFormat::Grouped;
        } else {
            std::cout << "Error: Invalid value for " << ArgOpt_output_format << ": " << format << "\n";
            return 1;
        }
    }

    // Set default values for AllVsAll with full content comparison
    // to improve performance when the user hasn't provided custom settings.
    if (config.operationMode == AntSeek::Config::OperationMode::AllVsAll &&
        config.matchContent == AntSeek::Config::MatchContent::Full) {
        if (config.hashMode == AntSeek::Config::HashMode::None) {
            config.hashMode = AntSeek::Config::HashMode::First;
        }
        config.matchSize = true;
    }

    try {
        // TODO: Implement smarter thread allocation based on task type, with customizable thread count via command-line argument.
        AntSeek::ThreadConfig thrCfg;
        int availableThreads = std::thread::hardware_concurrency();
        thrCfg.fileCollectorCount = std::max(1, availableThreads / 3);
        thrCfg.hashCalculatorCount = std::max(1, availableThreads / 3);
        thrCfg.comparerCount = std::max(1, availableThreads / 3);

        AntSeek as(config);
        as.start(thrCfg);

        // TODO: Implement a progress indicator.

        as.waitForFinish();
        as.printResults();
    }
    catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}