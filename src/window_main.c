// SPDX-License-Identifier: AGPL-3.0-only
// The calculator's window program: main, handing over to the program's
// entry (src/main_window.cc), which is compiled in a library.
int calculator_window_main(int argc, char **argv);

int main(int argc, char **argv) { return calculator_window_main(argc, argv); }
