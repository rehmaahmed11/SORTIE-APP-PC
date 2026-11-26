#!/usr/bin/env bash
# Builds AMS Sortie on Linux / macOS.
# Needs Qt 6.4+ (Core, Gui, Widgets, Svg) and CMake 3.21+.
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo
echo "Done. Run: $BUILD_DIR/SortieApp"
