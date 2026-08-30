# Android & Termux specific build configuration

message(STATUS "Configuring for Android target")

add_compile_definitions(
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
    PROOT_ANDROID
)

# Termux default temp directory and ashmem support
if(EXISTS "/data/data/com.termux" OR DEFINED ENV{TERMUX_VERSION})
    add_compile_definitions(
        PROOT_DEFAULT_TMP_DIR="/data/data/com.termux/files/usr/tmp"
        PROOT_UNBUNDLE_LOADER="/data/data/com.termux/files/usr/libexec/proot"
    )
endif()

# Check for Android shared memory (libandroid-shmem)
find_library(ANDROID_SHMEM_LIB NAMES android-shmem PATHS /data/data/com.termux/files/usr/lib)
if(ANDROID_SHMEM_LIB)
    add_compile_definitions(WITH_LIBANDROID_SHMEM)
    set(OS_EXTRA_LIBS ${OS_EXTRA_LIBS} ${ANDROID_SHMEM_LIB})
endif()

# Linker flags for Android
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack")
set(OS_EXTRA_LIBS ${OS_EXTRA_LIBS} dl)
