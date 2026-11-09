#!/usr/bin/env bash
# The static build's sysroot (cmake/musl-static.cmake), made with the
# compiler the calculator is built with (clang 23):
#   - Linux's UAPI headers, as the host has them;
#   - musl, static only;
#   - LLVM's runtimes against it: compiler-rt's builtins and CRT objects,
#     libunwind, libc++abi and libc++ (with its std module, for import std),
#     static only, from LLVM's release branch of the compiler's version.
# musl, libunwind, libc++abi and libc++ are compiled as LLVM bitcode
# (-flto=full): at the program's link they are one module with it, and what
# it calls in them is inlined (tools/force-inline). compiler-rt's builtins
# stay machine code: they are what the code generator calls.
#
#   musl-sysroot.sh <root>
set -euo pipefail
root=$1
triple=x86_64-unknown-linux-musl
musl_version=1.2.5
llvm_bin=/usr/lib/llvm-23/bin
lto=-flto=full
jobs=$(nproc)

mkdir -p "$root/usr/include" "$root/src"
# Linux's UAPI headers: the kernel's interface, the same for any C library.
cp -a /usr/include/linux /usr/include/asm-generic "$root/usr/include/"
cp -a /usr/include/x86_64-linux-gnu/asm "$root/usr/include/asm"
for dir in sound video drm misc mtd rdma scsi xen; do
  if [ -d "/usr/include/$dir" ]; then cp -a "/usr/include/$dir" "$root/usr/include/"; fi
done

# clang's own headers, and where compiler-rt is put: a resource directory
# of the sysroot's (-resource-dir), not the host compiler's.
resource="$root/clang-resource"
mkdir -p "$resource"
cp -a "$("$llvm_bin/clang" -print-resource-dir)/include" "$resource/"

cd "$root/src"
curl -fsSL "https://musl.libc.org/releases/musl-$musl_version.tar.gz" | tar xz
(
  cd "musl-$musl_version"
  CC="$llvm_bin/clang --target=$triple -resource-dir=$resource" \
  CFLAGS="-O3 $lto" AR="$llvm_bin/llvm-ar" RANLIB="$llvm_bin/llvm-ranlib" \
    ./configure --prefix="$root/usr" --target="$triple" --disable-shared --syslibdir="$root/lib"
  make -j"$jobs" install
)

git clone --quiet --depth 1 --branch release/23.x --filter=blob:none --sparse \
  https://github.com/llvm/llvm-project.git
git -C llvm-project sparse-checkout set runtimes libcxx libcxxabi libunwind compiler-rt cmake llvm/cmake llvm/utils/llvm-lit third-party libc

flags="-resource-dir=$resource"
cmake -S llvm-project/runtimes -B runtimes-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$root/usr" \
  -DCMAKE_C_COMPILER="$llvm_bin/clang" -DCMAKE_CXX_COMPILER="$llvm_bin/clang++" -DCMAKE_ASM_COMPILER="$llvm_bin/clang" \
  -DCMAKE_C_COMPILER_TARGET="$triple" -DCMAKE_CXX_COMPILER_TARGET="$triple" -DCMAKE_ASM_COMPILER_TARGET="$triple" \
  -DCMAKE_SYSROOT="$root" \
  -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_ASM_FLAGS="$flags" \
  -DCMAKE_AR="$llvm_bin/llvm-ar" -DCMAKE_RANLIB="$llvm_bin/llvm-ranlib" -DCMAKE_NM="$llvm_bin/llvm-nm" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DLLVM_ENABLE_RUNTIMES="compiler-rt;libunwind;libcxxabi;libcxx" \
  -DLLVM_DEFAULT_TARGET_TRIPLE="$triple" \
  -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
  -DCOMPILER_RT_INSTALL_PATH="$resource" \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
  -DCOMPILER_RT_BUILD_BUILTINS=ON -DCOMPILER_RT_BUILD_CRT=ON \
  -DCOMPILER_RT_BUILD_SANITIZERS=OFF -DCOMPILER_RT_BUILD_XRAY=OFF -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
  -DCOMPILER_RT_BUILD_PROFILE=OFF -DCOMPILER_RT_BUILD_MEMPROF=OFF -DCOMPILER_RT_BUILD_ORC=OFF \
  -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
  -DLIBUNWIND_ENABLE_SHARED=OFF -DLIBUNWIND_USE_COMPILER_RT=ON -DLIBUNWIND_INCLUDE_TESTS=OFF \
  -DLIBUNWIND_ADDITIONAL_COMPILE_FLAGS="$lto" \
  -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBCXXABI_USE_LLVM_UNWINDER=ON -DLIBCXXABI_USE_COMPILER_RT=ON \
  -DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBCXXABI_ADDITIONAL_COMPILE_FLAGS="$lto" \
  -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_HAS_MUSL_LIBC=ON -DLIBCXX_USE_COMPILER_RT=ON \
  -DLIBCXX_INSTALL_MODULES=ON -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DLIBCXX_ADDITIONAL_COMPILE_FLAGS="$lto"
cmake --build runtimes-build -j"$jobs"
cmake --install runtimes-build

test -f "$root/usr/lib/libc.a"
test -f "$root/usr/lib/libc++.a"
test -f "$root/usr/lib/libc++.modules.json"
rm -rf "$root/src"
