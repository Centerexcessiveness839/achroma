#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"

cmake -S "$ROOT" -B "$BUILD" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "$BUILD" --parallel "$(nproc)"

ln -sf "$BUILD/compile_commands.json" "$ROOT/compile_commands.json" 2>/dev/null || true

echo "[achroma] build complete: $BUILD/Achroma"
