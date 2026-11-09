// SPDX-License-Identifier: AGPL-3.0-only
// The calculator in a window of the system's -- the desktop's, Android's --
// drawn by Skia from the same tree as the page (skiff.app.window).
import std;
import skiff.app.window;
import calculator.page;
#include <skiff/app_window.h>

SKIFF_APP_WINDOW(calculator::page, calculator::Hooks)
