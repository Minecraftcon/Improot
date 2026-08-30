#!/usr/bin/env bash
# ==============================================================================
# Improot & PRoot Performance Benchmarking Script
# Auto-detects runtime engine (Native / PRoot / Improot / JIT / vdisk)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCH_BIN="/tmp/improot_bench_engine"

echo "========================================================================"
echo "          COMPILING BENCHMARK ENGINE INSIDE CONTAINER..."
echo "========================================================================"

if command -v gcc >/dev/null 2>&1; then
    gcc -O3 -Wall -Wextra -pthread "$SCRIPT_DIR/bench_engine.c" -o "$BENCH_BIN"
elif command -v clang >/dev/null 2>&1; then
    clang -O3 -Wall -Wextra -pthread "$SCRIPT_DIR/bench_engine.c" -o "$BENCH_BIN"
elif command -v cc >/dev/null 2>&1; then
    cc -O3 "$SCRIPT_DIR/bench_engine.c" -o "$BENCH_BIN"
else
    echo "[!] No C compiler found in container. Please install gcc/clang."
    exit 1
fi

echo "[+] Compilation successful: $BENCH_BIN"
echo ""

# Execute benchmark
"$BENCH_BIN" "$@"
