#!/usr/bin/env bash
set -e

PROOT="./Improot/src/proot"
PASSED=0
FAILED=0

test_case() {
    local name="$1"
    local img="$2"
    local cmd="$3"

    echo -e "\n========================================================"
    echo " TEST: ${name}"
    echo " IMAGE: ${img}"
    echo " COMMAND: ${cmd}"
    echo "========================================================"

    if [ ! -f "${img}" ]; then
        echo "[SKIP] Image ${img} not found."
        return
    fi

    if ${PROOT} --vdisk="${img}" -0 /bin/sh -c "export PATH=/bin:/usr/bin:/sbin:/usr/sbin:\$PATH; ${cmd}" 2>&1 || \
       ${PROOT} --vdisk="${img}" -0 /bin/bash -c "export PATH=/bin:/usr/bin:/sbin:/usr/sbin:\$PATH; ${cmd}" 2>&1; then
        echo "[PASS] ${name}"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] ${name}"
        FAILED=$((FAILED + 1))
    fi
}

echo "========================================================"
echo " Starting Multi-Format Virtual Disk Test Suite"
echo "========================================================"

# Test 1: Ubuntu VHD
test_case "Ubuntu (VHD) - Boot, Info & Listings" "tests/ubuntu_base.vhd" \
    "cat /etc/os-release | grep PRETTY_NAME && ls -la /root && ls -la /usr/bin | head -n 5"

# Test 2: Ubuntu QCOW2
test_case "Ubuntu (QCOW2) - Boot, Info & Listings" "tests/ubuntu_base.qcow2" \
    "cat /etc/os-release | grep PRETTY_NAME && ls /bin | head -n 5"

# Test 3: Ubuntu RAW
test_case "Ubuntu (RAW) - Boot, Info & Zero-Copy mmap" "tests/ubuntu_base.raw" \
    "cat /etc/os-release | grep PRETTY_NAME && uname -a"

# Test 4: Alpine VHD (musl libc)
test_case "Alpine Linux (VHD musl) - Boot & Execution" "examples/alpine_base.vhd" \
    "/bin/busybox uname -a && /bin/busybox ls -la /bin | /bin/busybox head -n 5"

# Test 5: Void Linux QCOW2 (xbps)
test_case "Void Linux (QCOW2 xbps) - Boot & Execution" "examples/void_base.qcow2" \
    "cat /etc/os-release | grep PRETTY_NAME && ls -la /usr/bin | head -n 5"

# Test 6: Fedora VHD (RPM/dnf)
test_case "Fedora Linux (VHD) - Boot & Execution" "examples/fedora_base.vhd" \
    "cat /etc/os-release | grep PRETTY_NAME && uname -a"

# Test 7: CirrOS Rootfs RAW (Buildroot)
test_case "CirrOS (RAW Buildroot) - Boot & Execution" "cirros_rootfs.img" \
    "cat /etc/os-release | grep PRETTY_NAME && uname -a"

# Test 8: Persistence Commit Test (Write, Exit, Re-read)
echo -e "\n========================================================"
echo " TEST: Persistence Commit & Verify on Ubuntu VHD"
echo "========================================================"
TOKEN="test_persistence_token_$(date +%s)"
${PROOT} --vdisk="tests/ubuntu_base.vhd" --commit -0 /bin/bash -c "echo '${TOKEN}' > /root/automated_token.txt"
READ_TOKEN=$(${PROOT} --vdisk="tests/ubuntu_base.vhd" -0 /bin/bash -c "cat /root/automated_token.txt")

if [ "${READ_TOKEN}" = "${TOKEN}" ]; then
    echo "[PASS] Persistence Commit Test (Token verified: ${READ_TOKEN})"
    PASSED=$((PASSED + 1))
else
    echo "[FAIL] Persistence Commit Test (Expected: ${TOKEN}, got: ${READ_TOKEN})"
    FAILED=$((FAILED + 1))
fi

echo -e "\n========================================================"
echo " TEST SUMMARY: ${PASSED} Passed, ${FAILED} Failed"
echo "========================================================"

if [ ${FAILED} -gt 0 ]; then
    exit 1
fi
