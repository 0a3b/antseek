# AntSeek - Group and Report Identical Files

AntSeek is a fast, multi-threaded tool that scans directories to group and list files with identical characteristics found in multiple locations. It compares files based on their name, size, hash, or content, with optimizations for speed. AntSeek only compares files that meet the given parameters, ensuring minimal resource usage while delivering accurate results.

## Features

* **Multi-threaded and optimized** for speed and efficiency.
* Supports various file comparison methods:
  * **Filename matching**: Finds files with matching names.
  * **Size matching**: Finds files with identical sizes.
  * **Hash-based comparison**: Compares files based on their hash (first or last N bytes).
  * **Content comparison**: Compares files by content, supporting full, partial (begin, end), or search-based comparisons.
* Configurable output formats (`pipe`, `tsv`, or `grouped`).
* Only compares the necessary files to improve performance.

## Installation

AntSeek is built using CMake, and it can be compiled and run on both **Linux** and **Windows** systems.

### Linux

Clone the repository and build using the provided script:

```bash
git clone https://github.com/0a3b/antseek.git
cd antseek
./build.sh
./build/antseek --help
```

### Windows

1. Download or clone the repository.
2. Open the project in Visual Studio.
3. Build the project in Debug or Release mode.
4. Run the generated `.exe` from the build output folder:

```cmd
antseek.exe --help
```

## Tested Compilers and Environments

AntSeek has been compiled and tested on several Linux distributions and Windows with the following compiler versions:

### Linux

| Distribution | Clang Version | GCC Version | Status                |
|--------------|---------------|-------------|-----------------------|
| Ubuntu       | 18.1.3        | 13.3.0      | Compiles successfully |
| Alpine       | 20.1.5        | 14.2.0      | Compiles successfully |
| Fedora       | 20.1.5        | 15.1.1      | Compiles successfully |
| Arch Linux   | 19.1.7        | 15.1.1      | Compiles successfully |
| Pop!_OS      | 14.0.0        | 11.4.0      | Fails to compile      |

### Windows

- MSVC (Visual Studio 2022) — Compiles successfully

## Usage

### Basic Command-Line Options

```
Usage: antseek --directories <dir1> <dir2> ... --filenames <pattern1> <pattern2> ...
--help                                     Show this help message
--version                                  Show version information
--output-format <pipe|tsv|grouped>         Output format (default: pipe)
--directories <dir1> <dir2> ...            Directories to process
--filenames <pattern1> <pattern2> ...      Filename patterns to match (expects C++ regex syntax)
--match-filenames                          Match files based on their filenames
--match-size                               Match files based on their size
--match-hash <first|last> <size>           Compare files by hashing the first or last N bytes (default: 4k)
--compare-content <full|begin|end|find>    Enables file comparison based on content.
                                             - full: Compares the full content of each file.
                                             - begin, end, find: Must be used together with the --compare-to option.
                                               - begin: Checks if the specified file's content appears at the beginning of each target file.
                                               - end: Checks if the specified file's content appears at the end of each target file.
                                               - find: Searches for the specified file's content anywhere within each target file.
--compare-to <file>                        Compare files based on the specified file's content.
--set-joker <value>                        Hexadecimal joker value to ignore during comparison (e.g. 0x000000FF; high-order bytes first).
--compare-everything                       Compare each file against every other file.
```

> When `--compare-everything` and `--compare-content full` are used, the program implicitly activates both `--match-size` and `--match-hash first` with a default hash block size of 4 KB.

## Example Use Cases

### 1. List `.txt` files from two directories

#### Windows

```bash
antseek --directories c:\temp c:\mystuff --filenames ".*\.txt$"
```

#### Linux

```bash
./antseek --directories ~/temp ~/mystuff --filenames ".*\.txt$"
```

---

### 2. Find `capture_[6-8 digits date].jpg/jpeg` images with identical size and hash (first 2KB)

#### Windows

```bash
antseek --directories c:\temp --filenames "^capture_\d{6,8}\.(jpg|jpeg)$" --compare-everything --match-size --match-hash first 2K
```

#### Linux

```bash
./antseek --directories ~/temp --filenames "^capture_\d{6,8}\.(jpg|jpeg)$" --compare-everything --match-size --match-hash first 2K
```

---

### 3. Perform full byte-by-byte content comparison of `.exe` and `.src` files

#### Windows

```bash
antseek --directories c:\temp --filenames ".*\.(exe|src)$" --compare-everything --compare-content full
```

#### Linux

```bash
./antseek --directories ~/temp --filenames ".*\.(exe|src)$" --compare-everything --compare-content full
```

## Output Formats

AntSeek supports the following output formats:

* `pipe` (default): Pipe-separated
* `tsv`: Tab-separated values
* `grouped`: Groups similar files together

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

This project was developed by 0a3b

## Third-party Licenses

This project uses the following third-party components:

- **xxHash** - [BSD 2-Clause License](external/xxhash/LICENSE)
  Source: https://github.com/Cyan4973/xxHash