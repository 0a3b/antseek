#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${2:-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building project in '$BUILD_DIR' directory with type '$BUILD_TYPE'..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE "$SCRIPT_DIR"
cmake --build .

echo "Build completed."
