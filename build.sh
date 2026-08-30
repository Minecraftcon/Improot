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
        CFLAGS="-O3 -Wall -Wextra -D_GNU_SOURCE -fno-stack-protector ${CFLAGS}"
        ;;

    *)
        echo "Unknown target: ${TARGET_ARCH}"
        echo "Supported: native, aarch64, arm32, android-arm64, android-arm32, termux"
        exit 1
        ;;
esac

# 2. Locate or auto-clone libvdisk
echo -e "\n[*] Locating libvdisk..."
LIBVDISK_DIR=""
if [ -d "${ROOT_DIR}/libvdisk" ]; then
    LIBVDISK_DIR="${ROOT_DIR}/libvdisk"
elif [ -d "${SCRIPT_DIR}/deps/libvdisk" ]; then
    LIBVDISK_DIR="${SCRIPT_DIR}/deps/libvdisk"
elif [ -d "${SCRIPT_DIR}/libvdisk" ]; then
    LIBVDISK_DIR="${SCRIPT_DIR}/libvdisk"
else
    echo "[*] libvdisk not found locally. Auto-cloning from https://github.com/Minecraftcon/libvdisk.git ..."
    mkdir -p "${SCRIPT_DIR}/deps"
    git clone https://github.com/Minecraftcon/libvdisk.git "${SCRIPT_DIR}/deps/libvdisk"
    LIBVDISK_DIR="${SCRIPT_DIR}/deps/libvdisk"
fi

# Reset MAKEFLAGS to prevent inherited broken jobserver pipes on Android/Termux
export MAKEFLAGS=""
NJOBS="$(nproc 2>/dev/null || echo 2)"

echo "[*] Building libvdisk at ${LIBVDISK_DIR}..."
make -C "${LIBVDISK_DIR}" clean
CC="${CC}" AR="${AR}" RANLIB="${RANLIB}" CFLAGS="${CFLAGS}" make -C "${LIBVDISK_DIR}" -j"${NJOBS}" || \
CC="${CC}" AR="${AR}" RANLIB="${RANLIB}" CFLAGS="${CFLAGS}" make -C "${LIBVDISK_DIR}"

# 3. Ensure talloc is available
echo -e "\n[*] Checking talloc library..."
if [ "${TARGET_ARCH}" = "termux" ]; then
    echo "[*] Using Termux talloc..."
elif [ ! -f "${SCRIPT_DIR}/lib/talloc/dist/lib/libtalloc.so" ] && ! ${CC} -ltalloc -o /dev/null -x c - <<< "int main(){}" 2>/dev/null; then
    echo "[*] System talloc not found. Building talloc from source..."
    cd "${SCRIPT_DIR}/lib/talloc"
    ./configure --prefix="$(pwd)/dist" --disable-python >/dev/null 2>&1
    make -j"${NJOBS}" >/dev/null 2>&1 || make >/dev/null 2>&1
    make install >/dev/null 2>&1
    cd "${SCRIPT_DIR}"
fi

# 4. Build Improot
echo -e "\n[*] Building Improot..."
cd "${SCRIPT_DIR}/src"
make clean || true
LIBVDISK_DIR="${LIBVDISK_DIR}" CC="${CC}" LD="${CC}" STRIP="${STRIP}" CFLAGS="${CFLAGS}" make -j"${NJOBS}" || \
LIBVDISK_DIR="${LIBVDISK_DIR}" CC="${CC}" LD="${CC}" STRIP="${STRIP}" CFLAGS="${CFLAGS}" make

echo -e "\n================================================================================"
echo " Build successful!"
echo " Output binary: ${SCRIPT_DIR}/src/proot"
echo " libvdisk:      ${LIBVDISK_DIR}/build/libvdisk.a"
echo "================================================================================"
