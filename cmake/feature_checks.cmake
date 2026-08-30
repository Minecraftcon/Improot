# Feature and capability detection for Improot

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

check_include_file("sys/user.h" HAVE_SYS_USER_H)
if(HAVE_SYS_USER_H)
    add_compile_definitions(HAVE_SYS_USER_H)
endif()

check_include_file("linux/ptrace.h" HAVE_LINUX_PTRACE_H)
if(HAVE_LINUX_PTRACE_H)
    add_compile_definitions(HAVE_LINUX_PTRACE_H)
endif()

# Check for talloc library
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PKG_TALLOC talloc)
    if(PKG_TALLOC_FOUND)
        set(TALLOC_LIBRARIES ${PKG_TALLOC_LIBRARIES})
        set(TALLOC_INCLUDE_DIRS ${PKG_TALLOC_INCLUDE_DIRS})
        set(TALLOC_FOUND TRUE)
    endif()
endif()

if(NOT TALLOC_FOUND)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/talloc/dist/lib/libtalloc.so" AND NOT ANDROID AND NOT DEFINED ENV{TERMUX_VERSION})
        add_library(talloc SHARED IMPORTED GLOBAL)
        set_target_properties(talloc PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/talloc/dist/lib/libtalloc.so"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/talloc/dist/include"
        )
        set(TALLOC_LIBRARIES talloc)
        set(TALLOC_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lib/talloc/dist/include")
        set(TALLOC_FOUND TRUE)
    else()
        find_path(TALLOC_INCLUDE_DIRS talloc.h PATHS /usr/include /data/data/com.termux/files/usr/include)
        find_library(TALLOC_LIBRARIES NAMES talloc PATHS /usr/lib /usr/lib64 /data/data/com.termux/files/usr/lib)
        if(NOT TALLOC_INCLUDE_DIRS OR TALLOC_INCLUDE_DIRS MATCHES "-NOTFOUND")
            set(TALLOC_INCLUDE_DIRS "")
        endif()
        if(NOT TALLOC_LIBRARIES OR TALLOC_LIBRARIES MATCHES "-NOTFOUND")
            set(TALLOC_LIBRARIES talloc)
        endif()
    endif()
endif()

# Check for libz
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    find_path(ZLIB_INCLUDE_DIRS zlib.h PATHS /usr/include /data/data/com.termux/files/usr/include)
    find_library(ZLIB_LIBRARIES NAMES z PATHS /usr/lib /usr/lib64 /data/data/com.termux/files/usr/lib)
endif()
if(NOT ZLIB_LIBRARIES)
    set(ZLIB_LIBRARIES z)
endif()

# Check for process_vm_readv
check_symbol_exists(process_vm_readv "sys/uio.h" HAVE_PROCESS_VM_READV)
if(HAVE_PROCESS_VM_READV)
    set(HAVE_PROCESS_VM ON)
else()
    set(HAVE_PROCESS_VM OFF)
endif()

# Check for ashmem/memfd
check_symbol_exists(memfd_create "sys/mman.h" HAVE_MEMFD_CREATE)
