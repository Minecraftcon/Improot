#!/usr/bin/env bash
# ==============================================================================
# Improot & libvdisk — Multi-Architecture Build Script
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
BUILD_TYPE="${2:-release}"

echo "================================================================================"
echo " Building Improot & libvdisk for target: ${TARGET_ARCH} (${BUILD_TYPE})"
echo "================================================================================"

# 1. Detect architecture and set toolchain flags
case "${TARGET_ARCH}" in
    native)
        echo "[*] Using native toolchain..."
        CC="${CC:-gcc}"
        AR="${AR:-ar}"
        RANLIB="${RANLIB:-ranlib}"
        STRIP="${STRIP:-strip}"
        ;;

    aarch64|arm64)
        echo "[*] Configuring for AArch64 (64-bit ARM / Termux)..."
        CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
        CC="${CROSS_COMPILE}gcc"
        AR="${CROSS_COMPILE}ar"
        RANLIB="${CROSS_COMPILE}ranlib"
        STRIP="${CROSS_COMPILE}strip"
        CFLAGS="-DARCH_ARM64=1 -O3 -Wall -Wextra -D_GNU_SOURCE ${CFLAGS}"
        ;;

    arm32|armv7|arm|armhf)
        echo "[*] Configuring for ARM32 EABI (Android 9 era armv7l / 32-bit ARM)..."
        CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
        CC="${CROSS_COMPILE}gcc"
        AR="${CROSS_COMPILE}ar"
        RANLIB="${CROSS_COMPILE}ranlib"
        STRIP="${CROSS_COMPILE}strip"
        CFLAGS="-DARCH_ARM_EABI=1 -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16 -O3 -Wall -Wextra -D_GNU_SOURCE ${CFLAGS}"
        ;;

    android-arm64)
        echo "[*] Configuring for Android NDK AArch64..."
        if [ -z "${ANDROID_NDK_HOME}" ] && [ -z "${NDK}" ]; then
            echo "ERROR: Set ANDROID_NDK_HOME or NDK environment variable for Android NDK builds."
            exit 1
        fi
        NDK_PATH="${ANDROID_NDK_HOME:-$NDK}"
        TOOLCHAIN="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin"
        CC="${TOOLCHAIN}/aarch64-linux-android28-clang"
        AR="${TOOLCHAIN}/llvm-ar"
        RANLIB="${TOOLCHAIN}/llvm-ranlib"
        STRIP="${TOOLCHAIN}/llvm-strip"
        CFLAGS="-DARCH_ARM64=1 -D__ANDROID__=1 -O3 -Wall -Wextra -D_GNU_SOURCE ${CFLAGS}"
        ;;

    android-arm32)
        echo "[*] Configuring for Android NDK ARM32 (Android 9 API 28)..."
        if [ -z "${ANDROID_NDK_HOME}" ] && [ -z "${NDK}" ]; then
            echo "ERROR: Set ANDROID_NDK_HOME or NDK environment variable for Android NDK builds."
            exit 1
        fi
        NDK_PATH="${ANDROID_NDK_HOME:-$NDK}"
        TOOLCHAIN="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin"
        CC="${TOOLCHAIN}/armv7a-linux-androideabi28-clang"
        AR="${TOOLCHAIN}/llvm-ar"
        RANLIB="${TOOLCHAIN}/llvm-ranlib"
        STRIP="${TOOLCHAIN}/llvm-strip"
        CFLAGS="-DARCH_ARM_EABI=1 -D__ANDROID__=1 -march=armv7-a -mfloat-abi=softfp -mfpu=neon -O3 -Wall -Wextra -D_GNU_SOURCE ${CFLAGS}"
        ;;

    termux)
        echo "[*] Configuring for Termux on-device native build..."
        CC="${CC:-clang}"
        AR="${AR:-llvm-ar}"
        RANLIB="${RANLIB:-llvm-ranlib}"
        STRIP="${STRIP:-llvm-strip}"
        CFLAGS="-O3 -Wall -Wextra -D_GNU_SOURCE ${CFLAGS}"
        ;;

    *)
        echo "Unknown target: ${TARGET_ARCH}"
        echo "Supported: native, aarch64, arm32, android-arm64, android-arm32, termux"
        exit 1
        ;;
esac

# 2. Build libvdisk
echo -e "\n[*] Building libvdisk..."
make -C "${ROOT_DIR}/libvdisk" clean
CC="${CC}" AR="${AR}" RANLIB="${RANLIB}" CFLAGS="${CFLAGS}" make -C "${ROOT_DIR}/libvdisk" -j"$(nproc 2>/dev/null || echo 2)"

# 3. Build Improot
echo -e "\n[*] Building Improot..."
cd "${SCRIPT_DIR}/src"
make clean || true
CC="${CC}" LD="${CC}" STRIP="${STRIP}" CFLAGS="${CFLAGS}" make -j"$(nproc 2>/dev/null || echo 2)"

echo -e "\n================================================================================"
echo " Build successful!"
echo " Output binary: ${SCRIPT_DIR}/src/proot"
echo " libvdisk:      ${ROOT_DIR}/libvdisk/build/libvdisk.a"
echo "================================================================================"
