#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${2:-build}"
shift 2 || true
EXTRA_CMAKE_OPTIONS="$@"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building project in '$BUILD_DIR' directory with type '$BUILD_TYPE'..."
echo "Extra CMake options: $EXTRA_CMAKE_OPTIONS"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE $EXTRA_CMAKE_OPTIONS "$SCRIPT_DIR"
cmake --build .

echo "Build completed."
