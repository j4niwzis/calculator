// SPDX-License-Identifier: AGPL-3.0-only
// The calculator on Android: NativeActivity's entry point, given to SDL's
// fork, which runs the app on a thread of its own -- the same program as the
// desktop's (skiff.app.window), under another name than main.
#include <SDL3/SDL.h>
#include <SDL3/SDL_nativeactivity.h>
#include <android/native_activity.h>
#include <cstddef>

import std;
import skiff.app.window;
import calculator.page;
#include <skiff/app_window.h>

SKIFF_APP_WINDOW_AS(calculator_main, calculator::page, calculator::Hooks)

namespace {
int run(int argc, char **argv) { return calculator_main(argc, argv); }
}  // namespace

extern "C" __attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity *activity, void *saved,
                                                                                 std::size_t size) {
  SDL_AndroidNativeActivity(activity, saved, size, run);
}
