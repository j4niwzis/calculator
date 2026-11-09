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

project(calculator VERSION 1.0.0 LANGUAGES CXX)

add_library(calculator_engine STATIC)
target_sources(calculator_engine PUBLIC FILE_SET CXX_MODULES FILES src/number.cc src/real.cc src/symbolic.cc src/engine.cc)
target_compile_features(calculator_engine PUBLIC cxx_std_26)

# The tests: googletest, as C++ modules (import gtest), as skiff's own.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
  # skiff-widgets ahead of the framework's pin: its SendButton takes a Theme on
  # both backends, which the calculator's keys are drawn by.
  cme_declare_port(NAME skiff-widgets PROVIDES skiff-widgets skiff_widgets
    GITHUB_REPOSITORY j4niwzis/skiff-widgets
    GIT_TAG 0a784229c6075ada818ce5e76e0a30370fc3cb56
    VERSION 0.1.0)
  cme_declare_port(NAME skiff-framework PROVIDES skiff-framework skiff_framework
    GITHUB_REPOSITORY j4niwzis/skiff-framework
    GIT_TAG a90c69292bddceef979b735c48ac18ab5b088c87
    VERSION 0.1.0)
  # The page alone: the browser's own elements, no canvas (and none of
  # skiff's scene). Forced: a build directory configured with the canvas
  # keeps the option in its cache, whatever the default is now.
  set(SKIFF_FRAMEWORK_CANVAS OFF CACHE BOOL "skiff.app with the canvas" FORCE)
  # Nor a window: a native program here only writes the page out.
  set(SKIFF_FRAMEWORK_WINDOW OFF CACHE BOOL "skiff.platform.window" FORCE)
  cme_find_package(skiff-framework 0.1.0 REQUIRED)

  add_library(calculator_ui STATIC)
  target_sources(calculator_ui PUBLIC FILE_SET CXX_MODULES FILES src/page.cc)
  target_link_libraries(calculator_ui PUBLIC calculator_engine skiff::framework)

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
