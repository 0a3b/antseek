#!/bin/bash
set -e

BUILD_DIR=build

echo "Building project in '$BUILD_DIR' directory..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

echo "Build completed."
