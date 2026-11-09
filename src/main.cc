// SPDX-License-Identifier: AGPL-3.0-only
// The calculator, as a program of skiff.app: in a browser the page over the
// copy written when it was built; natively, the program that writes that
// copy (calculator static.html index.html).
import std;
import skiff.app;
import calculator.page;
#include <skiff/app.h>

SKIFF_APP(calculator::page, calculator::Hooks)
