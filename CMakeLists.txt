# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# The calculator: its engine (src/engine.cc), plain C++ with no dependency,
# and the app (src/page.cc, src/main.cc), a program of skiff-framework's
# skiff.app -- in a browser the page itself, natively the program that
# writes the page out before the WebAssembly has loaded.
#
# CALCULATOR_ENGINE_ONLY builds the engine and its tests alone: nothing of
# skiff is fetched or built (only googletest, for the tests).
option(CALCULATOR_ENGINE_ONLY "Only the engine and its tests: no UI, no dependencies" OFF)

# skiff-framework (and skiff and skiff-widgets through it) comes from
# cmake-everywhere, at the revision named below.
if(NOT CALCULATOR_ENGINE_ONLY AND NOT COMMAND cme_declare_port AND NOT CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()

project(calculator VERSION 1.0.0 LANGUAGES C CXX)

# On Android (the source-built toolchain mux uses): SDL's NativeActivity
# fork, said before any port is declared: cmake-everywhere reads its registry at the
# first declaration, and the first to declare a port is the one read. That toolchain says Linux for
# CMAKE_SYSTEM_NAME and gives MANDK_ROOT: an Android build is said so here,
# before any port reads ANDROID -- else the fork was declared after the
# registry's SDL had been, and skiff-framework took the build for desktop
# Linux.
if(MANDK_ROOT AND NOT ANDROID)
  set(ANDROID TRUE)
endif()
# Everything position-independent on Android, as mux has it: the library is
# shared, and every static library linked into it is compiled alike -- else
# the std module was made twice (with -fPIC and without), and a target
# seeing both stopped the build ("Disagreement of the location of the 'std'
# module").
if(ANDROID)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(ANDROID AND NOT CALCULATOR_ENGINE_ONLY)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/android.cmake)
endif()

add_library(calculator_engine STATIC)
target_sources(calculator_engine PUBLIC FILE_SET CXX_MODULES FILES src/number.cc src/real.cc src/symbolic.cc src/engine.cc)
target_compile_features(calculator_engine PUBLIC cxx_std_26)

# The tests: googletest, as C++ modules (import gtest), as skiff's own.
# Not where they cannot run: the browser, Android.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT ANDROID)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake)
  CPMAddPackage(
    NAME googletest_modules
    # An archive of the revision, not a clone: nothing of its submodules needed.
    URL https://github.com/j4niwzis/googletest-modules/archive/706920f1d230f0316d271466d8885735be3e300e.tar.gz
    OPTIONS
      "BUILD_GMOCK OFF"
      "gtest_build_modules ON"
      "gtest_build_tests OFF"
      "gtest_build_samples OFF"
      "INSTALL_GTEST OFF")
  # Its module compiled as C++26, as everything importing it is: a module
  # file made under another standard is refused (C++26 was disabled in it).
  set_target_properties(gtest gtest_main PROPERTIES CXX_STANDARD 26)
  enable_testing()
  add_executable(calculator_engine_test test/engine.cc)
  target_link_libraries(calculator_engine_test PRIVATE calculator_engine gtest gtest_main)
  set_target_properties(calculator_engine_test PROPERTIES CXX_SCAN_FOR_MODULES ON)
  add_test(NAME engine COMMAND calculator_engine_test)
endif()

if(NOT CALCULATOR_ENGINE_ONLY)
  # skiff at its own head, as skiff-widgets' is built against it (the scene's
  # text kinds, the routing that carries a root's own scope).
  cme_declare_port(NAME skiff PROVIDES skiff
    GITHUB_REPOSITORY j4niwzis/skiff
    GIT_TAG bb541bc656c3861d95944f51c29f01b16ee50328
    VERSION 0.1.0)
  # skiff-widgets ahead of the framework's pin: its SendButton takes a Theme on
  # both backends, which the calculator's keys are drawn by.
  cme_declare_port(NAME skiff-widgets PROVIDES skiff-widgets skiff_widgets
    GITHUB_REPOSITORY j4niwzis/skiff-widgets
    GIT_TAG 622ce38bbdbdd47abf11568f33284b1495384a4e
    VERSION 0.1.0)
  cme_declare_port(NAME skiff-framework PROVIDES skiff-framework skiff_framework
    GITHUB_REPOSITORY j4niwzis/skiff-framework
    GIT_TAG f7caaac0b9d89258babc21236df26ef058881ab9
    VERSION 0.1.0)
  # Two builds of one tree: the page (under Emscripten, and natively the
  # program that writes it out), with no scene and no window; and the app in
  # a window (CALCULATOR_WINDOW: the desktop, Android), drawn by skiff's
  # scene with Skia. Forced either way: a build directory keeps what it was
  # configured with in its cache.
  option(CALCULATOR_WINDOW "The calculator as an app in a window (SDL, Skia)" OFF)
  set(SKIFF_FRAMEWORK_CANVAS ${CALCULATOR_WINDOW} CACHE BOOL "skiff.app with the scene" FORCE)
  set(SKIFF_FRAMEWORK_WINDOW ${CALCULATOR_WINDOW} CACHE BOOL "skiff.platform.window" FORCE)
  # The window has no layout reports and no views' names in its binary:
  # skiff's telling what sticks out is a development aid. The page keeps
  # the names, as data-view, which its tests find the display by.
  if(CALCULATOR_WINDOW)
    set(SKIFF_LAYOUT_REPORTS OFF CACHE BOOL "skiff's layout reports and views' names" FORCE)
  else()
    set(SKIFF_LAYOUT_REPORTS ON CACHE BOOL "skiff's layout reports and views' names" FORCE)
  endif()
  # The window draws skiff's scene with Skia itself -- not skiff's stand-in
  # for Skia's types, which draws nothing (a build directory first made for
  # the page keeps it in its cache).
  if(CALCULATOR_WINDOW)
    set(SKIFF_UI ON CACHE BOOL "skiff.ui: the scene" FORCE)
    set(SKIFF_SKIA ON CACHE BOOL "skiff on Skia" FORCE)
  endif()
  cme_find_package(skiff-framework 0.1.0 REQUIRED)

  add_library(calculator_ui STATIC)
  target_sources(calculator_ui PUBLIC FILE_SET CXX_MODULES FILES src/page.cc)
  target_link_libraries(calculator_ui PUBLIC calculator_engine skiff::framework)

  if(CALCULATOR_WINDOW)
    # The app in a window.
    if(ANDROID)
      # libcalculator.so, NativeActivity's, packaged as mux's is: the APK.
      add_library(calculator SHARED src/main_android.cc)
      target_link_libraries(calculator PRIVATE calculator_ui skiff::app_window log)
      if(MANDK_ROOT)
        target_link_libraries(calculator PRIVATE c++ c++abi unwind)
      endif()
      target_link_options(calculator PRIVATE ${CALCULATOR_ANDROID_LINK_OPTIONS})
      set_target_properties(calculator PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
      include(${CMAKE_CURRENT_LIST_DIR}/cmake/android-apk.cmake)
      calculator_add_android_apk(calculator)
    else()
      # The program in a library, main alone in the executable: everything
      # that imports std compiled one way (see src/main_window.cc).
      add_library(calculator_window_program STATIC src/main_window.cc)
      target_link_libraries(calculator_window_program PUBLIC calculator_ui skiff::app_window)
      set_target_properties(calculator_window_program PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
      add_executable(calculator-window src/window_main.c)
      target_link_libraries(calculator-window PRIVATE calculator_window_program)
      # The static build (cmake/musl-static.cmake): every direct call that is
      # not recursion inlined at this link -- this one only, not the feature
      # tests' nor the ports' (tools/force-inline).
      if(CALCULATOR_FORCE_INLINE_PLUGIN)
        target_link_options(calculator-window PRIVATE "LINKER:--load-pass-plugin=${CALCULATOR_FORCE_INLINE_PLUGIN}")
      endif()
    endif()
  else()
  add_executable(calculator src/main.cc)
  target_link_libraries(calculator PRIVATE calculator_ui)
  skiff_app(calculator SHELL ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)

  # Natively: the page written out, and read back -- the display and every
  # key on it.
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_test(NAME page_written
      COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:calculator> -DOUT=${CMAKE_CURRENT_BINARY_DIR}/page-test
              -P ${CMAKE_CURRENT_SOURCE_DIR}/test/page_written.cmake)
  endif()
  endif()
endif()
