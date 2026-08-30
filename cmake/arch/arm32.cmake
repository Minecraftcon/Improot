# ARM32 (ARMv7l / ARM EABI) Architecture Configuration

message(STATUS "Configuring for ARM32 (ARMv7l)")

set(LOADER_ADDR "0x10000000")
set(LOADER_ARCH_FLAGS "-marm")
set(ARCH_HAS_32BIT OFF)

add_compile_definitions(
    ARCH_ARM_EABI
    _ARM_
    HAS_POKEDATA_WORKAROUND
)
