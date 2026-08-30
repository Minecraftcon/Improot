# x86 (i386 / i686) Architecture Configuration

message(STATUS "Configuring for 32-bit x86")

set(LOADER_ADDR "0xa0000000")
set(LOADER_ARCH_FLAGS "-mregparm=3")
set(ARCH_HAS_32BIT OFF)

add_compile_definitions(
    ARCH_X86
    _X86_
)
