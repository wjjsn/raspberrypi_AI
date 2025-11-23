set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_LINKER ld.lld)

set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)


set(SYSROOT "/mnt/g/code/raspberrypi/5/sysroot")
set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_INSTALL_PREFIX ${SYSROOT}/usr/local CACHE PATH "Install prefix for RPi" FORCE)

set(GCC_VERSION 12)

set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_SYSROOT}/usr/include/c++/${GCC_VERSION}
    ${CMAKE_SYSROOT}/usr/include/aarch64-linux-gnu/c++/${GCC_VERSION}
)
set(CMAKE_PREFIX_PATH
    "${SYSROOT}/usr"
    "${SYSROOT}/usr/lib/aarch64-linux-gnu"
)
set(CMAKE_PACKAGE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# 1. 可执行文件
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)
# 2. 共享库 (.so) - 
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)
# 3. 模块插件
set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)

# add_link_options(-v)
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

add_compile_options(
    -g3
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/mnt/g/code/raspberrypi/5/toolchain.cmake -G Ninja && cmake --build build --config release