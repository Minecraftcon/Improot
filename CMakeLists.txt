cmake_minimum_required(VERSION 3.16)
project(improot C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Options
option(BUILD_LIBVDISK "Build bundled libvdisk" ON)
option(BUILD_CARE "Build CARE utility" OFF)
option(ENABLE_PYTHON "Enable Python extension" OFF)

# Common compiler flags
add_compile_options(
    -Wall
    -Wextra
    -O3
)

# Detect Git version
execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_VERSION)
    set(GIT_VERSION "5.4.0")
endif()
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()
set(VERSION "${GIT_VERSION}-${GIT_COMMIT}")

# ----------------------------------------------------------------------
# Modular Includes: Feature Checks, OS, and Architecture
# ----------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/feature_checks.cmake)

# OS-Specific Dispatch
if(ANDROID OR CMAKE_SYSTEM_NAME MATCHES "Android" OR EXISTS "/data/data/com.termux")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/os/android.cmake)
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/os/linux.cmake)
endif()

# Architecture-Specific Dispatch
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arch/aarch64.cmake)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv7|armv7l|armv8l")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arch/arm32.cmake)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arch/x86_64.cmake)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arch/x86.cmake)
else()
    message(WARNING "Unknown processor ${CMAKE_SYSTEM_PROCESSOR}, defaulting to arm32 settings")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arch/arm32.cmake)
endif()

# Configure build.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/build.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/build.h
    @ONLY
)

# Strip, NM, and AWK program detection
find_program(STRIP_EXECUTABLE NAMES llvm-strip strip)
if(NOT STRIP_EXECUTABLE)
    set(STRIP_EXECUTABLE "strip")
endif()

find_program(NM_EXECUTABLE NAMES llvm-nm nm)
if(NOT NM_EXECUTABLE)
    set(NM_EXECUTABLE "nm")
endif()

find_program(AWK_EXECUTABLE NAMES gawk awk nawk mawk)
if(NOT AWK_EXECUTABLE)
    set(AWK_EXECUTABLE "awk")
endif()

# Dependencies: libvdisk
if(BUILD_LIBVDISK AND NOT TARGET vdisk)
    set(LIBVDISK_DIR "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libvdisk/CMakeLists.txt")
        set(LIBVDISK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libvdisk")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk/CMakeLists.txt")
        set(LIBVDISK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libvdisk/CMakeLists.txt")
        set(LIBVDISK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libvdisk")
    else()
        message(STATUS "Auto-cloning libvdisk from https://github.com/Minecraftcon/libvdisk.git...")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/deps")
        execute_process(
            COMMAND git clone https://github.com/Minecraftcon/libvdisk.git "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk"
            RESULT_VARIABLE GIT_CLONE_RES
        )
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk/CMakeLists.txt")
            set(LIBVDISK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk")
        endif()
    endif()

    if(LIBVDISK_DIR)
        add_subdirectory("${LIBVDISK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libvdisk" EXCLUDE_FROM_ALL)
    endif()
endif()

# ----------------------------------------------------------------------
# Freestanding Loader Build & Packaging
# ----------------------------------------------------------------------
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/loader.elf ${CMAKE_CURRENT_BINARY_DIR}/loader-info.c
    COMMAND ${CMAKE_C_COMPILER}
        -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -D_GNU_SOURCE
        -fPIC -ffreestanding -fno-stack-protector -fno-builtin ${LOADER_ARCH_FLAGS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/assembly.S
        -static -nostdlib
        -Wl,--build-id=none,-Ttext=${LOADER_ADDR},-z,noexecstack
        -o ${CMAKE_CURRENT_BINARY_DIR}/loader.elf
    COMMAND ${CMAKE_COMMAND} -E echo "Generating loader-info.c from loader.elf..."
    COMMAND ${NM_EXECUTABLE} -n ${CMAKE_CURRENT_BINARY_DIR}/loader.elf | ${AWK_EXECUTABLE} -f ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader-info.awk > ${CMAKE_CURRENT_BINARY_DIR}/loader-info.c || ${CMAKE_COMMAND} -E echo "const ssize_t offset_to_pokedata_workaround = 0;" > ${CMAKE_CURRENT_BINARY_DIR}/loader-info.c
    COMMAND ${STRIP_EXECUTABLE} -s ${CMAKE_CURRENT_BINARY_DIR}/loader.elf || true
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/assembly.S
        ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader-info.awk
    COMMENT "Compiling freestanding loader.elf and generating loader-info.c"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo ".global _binary_loader_elf_start" > ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo ".global _binary_loader_elf_end" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo ".section .rodata, \\\"a\\\"" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo "_binary_loader_elf_start:" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo ".incbin \\\"${CMAKE_CURRENT_BINARY_DIR}/loader.elf\\\"" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    COMMAND ${CMAKE_COMMAND} -E echo "_binary_loader_elf_end:" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/loader.elf
    COMMENT "Generating loader_wrapped.S"
)

set(LOADER_WRAPPED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/loader_wrapped.S)

# Optional 32-bit companion loader for 64-bit platforms
if(ARCH_HAS_32BIT)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -m32 -E -x c /dev/null
        RESULT_VARIABLE M32_CHECK_RES
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(M32_CHECK_RES EQUAL 0)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/loader-m32.elf
            COMMAND ${CMAKE_C_COMPILER}
                -m32
                -I${CMAKE_CURRENT_SOURCE_DIR}/src
                -D_GNU_SOURCE
                -fno-pic -fno-PIE -ffreestanding -fno-stack-protector -fno-builtin -mregparm=3
                ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader.c
                ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/assembly.S
                -static -nostdlib -Wl,-no-pie
                -Wl,--build-id=none,--image-base=0xa0000000,-Ttext=0xa0000000,-N,-z,noexecstack
                -o ${CMAKE_CURRENT_BINARY_DIR}/loader-m32.elf
            COMMAND ${STRIP_EXECUTABLE} -s ${CMAKE_CURRENT_BINARY_DIR}/loader-m32.elf || true
            DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/loader.c
                ${CMAKE_CURRENT_SOURCE_DIR}/src/loader/assembly.S
            COMMENT "Compiling freestanding 32-bit loader-m32.elf"
        )

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo ".global _binary_loader_m32_elf_start" > ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo ".global _binary_loader_m32_elf_end" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo ".section .rodata, \\\"a\\\"" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo "_binary_loader_m32_elf_start:" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo ".incbin \\\"${CMAKE_CURRENT_BINARY_DIR}/loader-m32.elf\\\"" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            COMMAND ${CMAKE_COMMAND} -E echo "_binary_loader_m32_elf_end:" >> ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/loader-m32.elf
            COMMENT "Generating loader_m32_wrapped.S"
        )

        list(APPEND LOADER_WRAPPED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/loader_m32_wrapped.S)
    endif()
endif()

# ----------------------------------------------------------------------
# JIT Shared Library: libimproot-jit.so
# ----------------------------------------------------------------------
add_library(improot-jit SHARED src/jit/jit_lib.c)
target_include_directories(improot-jit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_options(improot-jit PRIVATE
    -O3 -Wall -Wextra -fno-stack-protector -nostdlib -fPIC
)
set_target_properties(improot-jit PROPERTIES
    OUTPUT_NAME "improot-jit"
    PREFIX "lib"
    SUFFIX ".so"
    LINK_FLAGS "-nostdlib"
)

# ----------------------------------------------------------------------
# Main Executable: proot
# ----------------------------------------------------------------------
set(IMPROOT_SOURCES
    src/cli/cli.c
    src/cli/proot.c
    src/cli/note.c
    src/execve/enter.c
    src/execve/exit.c
    src/execve/shebang.c
    src/execve/elf.c
    src/execve/ldso.c
    src/execve/auxv.c
    src/execve/aoxp.c
    src/path/binding.c
    src/path/glue.c
    src/path/canon.c
    src/path/f2fs-bug.c
    src/path/path.c
    src/path/proc.c
    src/path/temp.c
    src/syscall/seccomp.c
    src/syscall/syscall.c
    src/syscall/chain.c
    src/jit/shm.c
    src/syscall/enter.c
    src/syscall/exit.c
    src/syscall/sysnum.c
    src/syscall/socket.c
    src/syscall/heap.c
    src/syscall/rlimit.c
    src/syscall/pipe_shadow.c
    src/tracee/tracee.c
    src/tracee/mem.c
    src/tracee/reg.c
    src/tracee/event.c
    src/tracee/seccomp.c
    src/tracee/statx.c
    src/ptrace/ptrace.c
    src/ptrace/user.c
    src/ptrace/wait.c
    src/extension/extension.c
    src/extension/ashmem_memfd/ashmem_memfd.c
    src/extension/kompat/kompat.c
    src/extension/fake_id0/chown.c
    src/extension/fake_id0/chroot.c
    src/extension/fake_id0/getsockopt.c
    src/extension/fake_id0/sendmsg.c
    src/extension/fake_id0/socket.c
    src/extension/fake_id0/open.c
    src/extension/fake_id0/unlink.c
    src/extension/fake_id0/rename.c
    src/extension/fake_id0/chmod.c
    src/extension/fake_id0/utimensat.c
    src/extension/fake_id0/access.c
    src/extension/fake_id0/exec.c
    src/extension/fake_id0/link.c
    src/extension/fake_id0/symlink.c
    src/extension/fake_id0/mk.c
    src/extension/fake_id0/stat.c
    src/extension/fake_id0/helper_functions.c
    src/extension/fake_id0/fake_id0.c
    src/extension/hidden_files/hidden_files.c
    src/extension/mountinfo/mountinfo.c
    src/extension/port_switch/port_switch.c
    src/extension/sysvipc/sysvipc.c
    src/extension/sysvipc/sysvipc_msg.c
    src/extension/sysvipc/sysvipc_sem.c
    src/extension/sysvipc/sysvipc_shm.c
    src/extension/link2symlink/link2symlink.c
    src/extension/vdisk/vdisk.c
    src/extension/portmap/portmap.c
    src/extension/portmap/map.c
    ${CMAKE_CURRENT_BINARY_DIR}/loader-info.c
    ${LOADER_WRAPPED_SOURCES}
)

add_executable(proot ${IMPROOT_SOURCES})

target_include_directories(proot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/uthash/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/uthash/include
    ${LIBVDISK_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/libvdisk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libvdisk/include
    ${TALLOC_INCLUDE_DIRS}
)

target_link_libraries(proot PRIVATE
    vdisk
    ${TALLOC_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${OS_EXTRA_LIBS}
)

# Optional CARE utility
if(BUILD_CARE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

    set(CARE_SOURCES
        ${IMPROOT_SOURCES}
        src/cli/care.c
        src/extension/care/care.c
        src/extension/care/final.c
        src/extension/care/extract.c
        src/extension/care/archive.c
    )

    add_executable(care ${CARE_SOURCES})
    target_include_directories(care PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/uthash/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/uthash/include
        ${TALLOC_INCLUDE_DIRS}
        ${LIBARCHIVE_INCLUDE_DIRS}
    )
    target_link_libraries(care PRIVATE
        vdisk
        ${TALLOC_LIBRARIES}
        ${LIBARCHIVE_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${OS_EXTRA_LIBS}
    )
endif()

# Installation
install(TARGETS proot improot-jit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
