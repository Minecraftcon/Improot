#!/usr/bin/env bash
# ==============================================================================
# Improot & libvdisk — CMake Multi-Architecture Build Script
# Supports:
#   - x86_64 / i686 (Native Linux)
#   - AArch64 (Termux / Android 64-bit / Linux arm64)
#   - ARM32 EABI (Android 9 era armv7l / Termux 32-bit / Linux armhf)
#   - Android NDK cross-compilation (API 28+ for Android 9, API 31+ for Android 12+)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

TARGET_ARCH="${1:-native}"
BUILD_TYPE="${2:-Release}"

echo "================================================================================"
echo " Building Improot & libvdisk with CMake for target: ${TARGET_ARCH} (${BUILD_TYPE})"
echo "================================================================================"

CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DBUILD_LIBVDISK=ON"
)

# 1. Configure toolchain options per architecture
case "${TARGET_ARCH}" in
    native)
        echo "[*] Using native toolchain..."
        ;;

    aarch64|arm64)
        echo "[*] Configuring for AArch64 cross-compilation..."
        CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
        CMAKE_ARGS+=(
            "-DCMAKE_SYSTEM_NAME=Linux"
            "-DCMAKE_SYSTEM_PROCESSOR=aarch64"
            "-DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc"
            "-DCMAKE_STRIP=${CROSS_COMPILE}strip"
            "-DCMAKE_C_FLAGS=-DARCH_ARM64=1 -O3 -D_GNU_SOURCE"
        )
        ;;

    arm32|armv7|arm|armhf)
        echo "[*] Configuring for ARM32 EABI cross-compilation..."
        CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
        CMAKE_ARGS+=(
            "-DCMAKE_SYSTEM_NAME=Linux"
            "-DCMAKE_SYSTEM_PROCESSOR=armv7l"
            "-DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc"
            "-DCMAKE_STRIP=${CROSS_COMPILE}strip"
            "-DCMAKE_C_FLAGS=-DARCH_ARM_EABI=1 -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16 -O3 -D_GNU_SOURCE"
        )
        ;;

    android-arm64)
        echo "[*] Configuring for Android NDK AArch64..."
        if [ -z "${ANDROID_NDK_HOME}" ] && [ -z "${NDK}" ]; then
            echo "ERROR: Set ANDROID_NDK_HOME or NDK environment variable for Android NDK builds."
            exit 1
        fi
        NDK_PATH="${ANDROID_NDK_HOME:-$NDK}"
        CMAKE_ARGS+=(
            "-DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake"
            "-DANDROID_ABI=arm64-v8a"
            "-DANDROID_PLATFORM=android-28"
            "-DCMAKE_C_FLAGS=-DARCH_ARM64=1 -D__ANDROID__=1"
        )
        ;;

    android-arm32)
        echo "[*] Configuring for Android NDK ARM32 (Android 9 API 28)..."
        if [ -z "${ANDROID_NDK_HOME}" ] && [ -z "${NDK}" ]; then
            echo "ERROR: Set ANDROID_NDK_HOME or NDK environment variable for Android NDK builds."
            exit 1
        fi
        NDK_PATH="${ANDROID_NDK_HOME:-$NDK}"
        CMAKE_ARGS+=(
            "-DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake"
            "-DANDROID_ABI=armeabi-v7a"
            "-DANDROID_PLATFORM=android-28"
            "-DCMAKE_C_FLAGS=-DARCH_ARM_EABI=1 -D__ANDROID__=1 -mfpu=neon"
        )
        ;;

    termux)
        echo "[*] Configuring for Termux on-device build..."
        CMAKE_ARGS+=(
            "-DCMAKE_C_COMPILER=${CC:-clang}"
            "-DCMAKE_STRIP=${STRIP:-llvm-strip}"
            "-DCMAKE_C_FLAGS=-O3 -D_GNU_SOURCE -fno-stack-protector"
        )
        ;;

    *)
        echo "Unknown target: ${TARGET_ARCH}"
        echo "Supported: native, aarch64, arm32, android-arm64, android-arm32, termux"
        exit 1
        ;;
esac

# 2. Run CMake configure & build
BUILD_DIR="${SCRIPT_DIR}/build"
NJOBS="$(nproc 2>/dev/null || echo 2)"

echo -e "\n[*] Configuring CMake in ${BUILD_DIR}..."
cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" "${CMAKE_ARGS[@]}"

echo -e "\n[*] Building Improot & libvdisk..."
cmake --build "${BUILD_DIR}" --parallel "${NJOBS}"

# Symlink output binary to src/proot for compatibility with scripts expecting legacy path
mkdir -p "${SCRIPT_DIR}/src"
ln -sf "${BUILD_DIR}/proot" "${SCRIPT_DIR}/src/proot"

echo -e "\n================================================================================"
echo " Build successful!"
echo " Output binary: ${BUILD_DIR}/proot (symlinked at ${SCRIPT_DIR}/src/proot)"
if [ -f "${BUILD_DIR}/libvdisk/libvdisk.a" ]; then
    echo " libvdisk:      ${BUILD_DIR}/libvdisk/libvdisk.a"
fi
echo "================================================================================"
