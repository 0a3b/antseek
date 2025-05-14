cmake_minimum_required(VERSION 3.14)
project(antseek)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
add_executable(antseek
    src/antseek.cpp
    external/xxhash/xxhash.c
)

# Include directories
target_include_directories(antseek PRIVATE
    include
    external/xxhash
)


