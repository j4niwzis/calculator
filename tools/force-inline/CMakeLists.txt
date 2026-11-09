# SPDX-License-Identifier: AGPL-3.0-only
# The ForceInline pass plugin (ForceInline.cpp), built against the LLVM the
# static build's lld is: llvm-23-dev.
cmake_minimum_required(VERSION 3.20)
project(force_inline CXX)
find_package(LLVM 23 REQUIRED CONFIG)
add_library(ForceInline MODULE ForceInline.cpp)
target_include_directories(ForceInline SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(force_inline_definitions NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_definitions(ForceInline PRIVATE ${force_inline_definitions})
target_compile_options(ForceInline PRIVATE -fno-rtti)
set_target_properties(ForceInline PROPERTIES CXX_STANDARD 17 PREFIX "")
