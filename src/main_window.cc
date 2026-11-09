// SPDX-License-Identifier: AGPL-3.0-only
// The calculator in a window of the system's -- the desktop's, Android's --
// drawn by Skia from the same tree as the page (skiff.app.window).
import std;
import skiff.app.window;
import calculator.page;
#include <skiff/app_window.h>

// The program's entry, called by main (src/window_main.c): this unit is a
// library's, so that every unit importing std is compiled the same way --
// an executable's are position-independent as an executable (-fPIE), a
// library's as a library (-fPIC), and the std module was made twice.
SKIFF_APP_WINDOW_AS(calculator_window_entry, calculator::page, calculator::Hooks)
// By the name main calls it by, in C.
extern "C" int calculator_window_main(int argc, char **argv) { return calculator_window_entry(argc, argv); }
