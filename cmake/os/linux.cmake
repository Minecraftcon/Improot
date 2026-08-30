# Standard Linux (glibc/musl) build configuration

message(STATUS "Configuring for Linux target")

add_compile_definitions(
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack")
set(OS_EXTRA_LIBS ${OS_EXTRA_LIBS} dl)
