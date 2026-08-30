# ARM32 (ARMv7l / ARM EABI) Architecture Configuration

message(STATUS "Configuring for ARM32 (ARMv7l)")

set(LOADER_ADDR "0x20000000")
set(LOADER_ARCH_FLAGS "-mthumb")
set(ARCH_HAS_32BIT OFF)

add_compile_definitions(
    ARCH_ARM_EABI
    _ARM_
)
