set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_LINKER ld.lld)

set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)


set(SYSROOT "/mnt/g/code/raspberrypi/4b/sysroot")
set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_PREFIX_PATH
    "${SYSROOT}/usr"
    "${SYSROOT}/usr/lib/aarch64-linux-gnu"
)
set(CMAKE_PACKAGE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

add_compile_options(
    -g3
)