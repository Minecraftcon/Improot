# x86_64 (AMD64) Architecture Configuration

message(STATUS "Configuring for x86_64 (AMD64)")

set(LOADER_ADDR "0x600000000000")
set(LOADER_ARCH_FLAGS "")
set(ARCH_HAS_32BIT ON)

add_compile_definitions(
    ARCH_X86_64
    _X86_64_
)
