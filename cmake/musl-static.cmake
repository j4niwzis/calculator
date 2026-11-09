# SPDX-License-Identifier: AGPL-3.0-only
# The static build's toolchain: one program, nothing loaded at run time.
#
# clang 23 for x86_64-unknown-linux-musl, against the sysroot
# .github/scripts/musl-sysroot.sh makes (CALCULATOR_SYSROOT): musl, libc++,
# libc++abi, libunwind, compiler-rt. Everything -- the calculator, every
# library cmake-everywhere builds for it, and the sysroot's libraries --
# compiled as LLVM bitcode (-flto=full) and optimised as one module at the
# link, where the ForceInline pass plugin (tools/force-inline,
# CALCULATOR_FORCE_INLINE_PLUGIN) inlines every direct call that is not
# recursion.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/musl-static.cmake \
#         -DCALCULATOR_SYSROOT=<root> -DCALCULATOR_FORCE_INLINE_PLUGIN=<ForceInline.so>
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT CALCULATOR_SYSROOT AND DEFINED ENV{CALCULATOR_SYSROOT})
  set(CALCULATOR_SYSROOT "$ENV{CALCULATOR_SYSROOT}")
endif()
if(NOT CALCULATOR_FORCE_INLINE_PLUGIN AND DEFINED ENV{CALCULATOR_FORCE_INLINE_PLUGIN})
  set(CALCULATOR_FORCE_INLINE_PLUGIN "$ENV{CALCULATOR_FORCE_INLINE_PLUGIN}")
endif()
if(NOT CALCULATOR_SYSROOT)
  message(FATAL_ERROR "musl-static.cmake: CALCULATOR_SYSROOT is the sysroot musl-sysroot.sh made")
endif()
# Said again in every try_compile and in every port cmake-everywhere
# configures on its own.
set(ENV{CALCULATOR_SYSROOT} "${CALCULATOR_SYSROOT}")
if(CALCULATOR_FORCE_INLINE_PLUGIN)
  set(ENV{CALCULATOR_FORCE_INLINE_PLUGIN} "${CALCULATOR_FORCE_INLINE_PLUGIN}")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CALCULATOR_SYSROOT CALCULATOR_FORCE_INLINE_PLUGIN)

set(calculator_triple x86_64-unknown-linux-musl)
set(calculator_llvm /usr/lib/llvm-23/bin)
set(CMAKE_C_COMPILER "${calculator_llvm}/clang")
set(CMAKE_CXX_COMPILER "${calculator_llvm}/clang++")
set(CMAKE_ASM_COMPILER "${calculator_llvm}/clang")
set(CMAKE_C_COMPILER_TARGET ${calculator_triple})
set(CMAKE_CXX_COMPILER_TARGET ${calculator_triple})
set(CMAKE_ASM_COMPILER_TARGET ${calculator_triple})
set(CMAKE_SYSROOT "${CALCULATOR_SYSROOT}")
set(CMAKE_AR "${calculator_llvm}/llvm-ar")
set(CMAKE_RANLIB "${calculator_llvm}/llvm-ranlib")
set(CMAKE_NM "${calculator_llvm}/llvm-nm")
set(CMAKE_LINKER_TYPE LLD)

set(calculator_common "-resource-dir=${CALCULATOR_SYSROOT}/clang-resource -flto=full")
set(CMAKE_C_FLAGS_INIT "${calculator_common}")
set(CMAKE_ASM_FLAGS_INIT "-resource-dir=${CALCULATOR_SYSROOT}/clang-resource")
set(CMAKE_CXX_FLAGS_INIT "${calculator_common} -stdlib=libc++")
set(calculator_link "-static -fuse-ld=lld -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -Wl,--lto-O3")
if(CALCULATOR_FORCE_INLINE_PLUGIN)
  string(APPEND calculator_link " -Wl,--load-pass-plugin=${CALCULATOR_FORCE_INLINE_PLUGIN}")
endif()
set(CMAKE_EXE_LINKER_FLAGS_INIT "${calculator_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind")
# Static libraries only: a package that could build either builds this one.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Nothing of the machine's: every library is built here, from
# cmake-everywhere's ports (CME_SYSTEM=NEVER, said by the workflow), into
# prefixes of their own outside the sysroot -- so the find modes stay as
# they are, the ports' prefixes found where cmake-everywhere says. The
# machine's pkg-config files are never read.
set(ENV{PKG_CONFIG_LIBDIR} "${CALCULATOR_SYSROOT}/usr/lib/pkgconfig:${CALCULATOR_SYSROOT}/usr/share/pkgconfig")
