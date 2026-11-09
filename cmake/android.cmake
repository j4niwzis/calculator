# SPDX-License-Identifier: AGPL-3.0-only
# Android, as mux builds for it: the source-built toolchain (CMAKE_SYSTEM_NAME
# Linux, its ABI read from android/abis.json by the processor it says), SDL's
# NativeActivity fork linked in statically, the library's link options.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../android/abis.json" calculator_abis)
string(JSON calculator_abi_count LENGTH "${calculator_abis}")
math(EXPR calculator_abi_last "${calculator_abi_count} - 1")
unset(CALCULATOR_ANDROID_ABI)
foreach(index RANGE ${calculator_abi_last})
  string(JSON abi MEMBER "${calculator_abis}" ${index})
  string(JSON triple GET "${calculator_abis}" ${abi} target triple)
  string(JSON sysroot_triple ERROR_VARIABLE none GET "${calculator_abis}" ${abi} target sysroot-triple)
  if(none)
    set(sysroot_triple "${triple}")
  endif()
  string(REGEX REPLACE "-.*$" "" processor "${sysroot_triple}")
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL processor)
    set(CALCULATOR_ANDROID_ABI "${abi}")
  endif()
endforeach()
if(NOT DEFINED CALCULATOR_ANDROID_ABI)
  message(FATAL_ERROR "No Android ABI in android/abis.json is built for ${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CALCULATOR_ANDROID_NATIVE_APP_GLUE "${MANDK_ROOT}/src/ndk/sources/android/native_app_glue"
    CACHE PATH "AOSP native_app_glue source directory")
# SDL: the NativeActivity fork mux uses, at its pin.
cme_declare_port(NAME sdl3 PROVIDES SDL3 sdl3 VERSION 3.5.0
  GITHUB_REPOSITORY j4niwzis/SDL GIT_TAG 91c08de87b43fdecc824464fcce15499ccbc7101
  LICENSE Zlib TARGETS SDL3::SDL3)
# Static, in libcalculator.so: the DEX loads libcalculator.so, whose
# JNI_OnLoad is SDL's (android/exports.map).
cme_options(sdl3 "SDL_ANDROID_NATIVE_ACTIVITY ON" "SDL_ANDROID_JAR OFF"
  "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_INSTALL OFF" "SDL_VULKAN OFF"
  "SDL_ANDROID_NATIVE_APP_GLUE ${CALCULATOR_ANDROID_NATIVE_APP_GLUE}")
set(CME_SYSTEM_SDL3 OFF)

# Symbols hidden unless exported: android/exports.map says which.
string(APPEND CMAKE_C_FLAGS " -fvisibility=hidden")
string(APPEND CMAKE_CXX_FLAGS " -fvisibility=hidden -fvisibility-inlines-hidden")
# The link options of an Android library: 16 KB pages, nothing undefined,
# only android/exports.map exported; a build id, as the debug APK's symbols
# are matched by it.
set(CALCULATOR_ANDROID_LINK_OPTIONS "LINKER:-z,max-page-size=16384" "LINKER:--no-undefined"
  "LINKER:--version-script=${CMAKE_CURRENT_LIST_DIR}/../android/exports.map" "LINKER:--build-id=sha1")
