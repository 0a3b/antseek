cmake_minimum_required(VERSION 3.14)
project(antseek)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AddressSanitizer if requested and supported
if (ENABLE_ASAN)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "AddressSanitizer enabled")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
    else()
        message(WARNING "AddressSanitizer is not supported on this compiler")
    endif()
endif()

# Source files
add_executable(antseek
    src/main.cpp
    src/AntSeek.cpp
    external/xxhash/xxhash.c
)

# Include directories
target_include_directories(antseek PRIVATE
    include
    external/xxhash
)


