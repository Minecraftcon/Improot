# AArch64 (ARM64) Architecture Configuration

message(STATUS "Configuring for AArch64 (ARM64)")

set(LOADER_ADDR "0x2000000000")
set(LOADER_ARCH_FLAGS "")
set(ARCH_HAS_32BIT OFF)

add_compile_definitions(
    ARCH_ARM64
    _AARCH64_
)
