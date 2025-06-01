#!/bin/bash
set -e

BUILD_DIR="${1:-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building project in '$BUILD_DIR' directory..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release "$SCRIPT_DIR"
cmake --build .

echo "Build completed."
