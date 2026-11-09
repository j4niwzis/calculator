# Calculator

A calculator for the browser, written in C++26 with
[skiff-framework](https://github.com/j4niwzis/skiff-framework). The page is
written out when the program is built, so it shows at once; the WebAssembly
then takes it over.

## Using it

It works as Android's calculator does: you type the whole formula, its
result shows under it as you type, and `=` makes the result the number.

| Key | Mouse | Keyboard |
| --- | --- | --- |
| Digits, decimal point | `0`–`9`, `.` | `0`–`9`, `.` or `,` |
| Add, subtract, multiply, divide | `+` `−` `×` `÷` | `+` `-` `*` (or `x`) `/` |
| Parentheses (one key: opens, or closes one that is open) | `( )` | `(` or `)` |
| Power, factorial, percent | `^` `!` `%` | `^` `!` `%` |
| Square root, square | `√` `x²` | `r` |
| π, e | `π` `e` | `p` `e` |
| sin, cos, tan (`INV`: their inverses) | `sin` `cos` `tan` | `s` `c` `t` |
| ln, log₁₀ (`INV`: eˣ, 10ˣ) | `ln` `log` | `l` `g` |
| Radians / degrees | `RAD` / `DEG` | |
| Work it out | `=` | `Enter` or `=` |
| Delete the last digit or part | `⌫` | `Backspace` |
| Clear | `AC` | `Escape` or `Delete` |

The scientific keys show one row folded; the chevron opens all three.

How it works things out:

- **Precedence**, as written: `2+3×4` is 14, `2^3^2` is 512, `−2^2` is −4.
  Two things side by side are multiplied: `2π`, `3(4+1)`. A parenthesis
  left open is closed at the end.
- **Exact.** Every number is kept as Hans Boehm's calculator for Android
  keeps it: a rational of unbounded integers where it is one, so
  `0.1 + 0.2` is `0.3`, `10^100 + 1 − 10^100` is `1` and `1 ÷ 3 × 3` is `1`;
  a rational times π, a root, a power of e, a logarithm or a trigonometric
  value where it is not (π + 3π is 4π, √8 is 2√2, sin(π/6) is ½,
  ln 4 − 2 ln 2 is 0); and a constructive real, computed to whatever
  precision is asked, where nothing else fits.
- **Every digit shown is correct.** The result under the formula has 15
  significant digits, the number after `=` 60; a number that goes on is
  cut, never rounded, and ends in `…`. The value kept is exact, so what is
  done next with it is exact too.
- **Errors**: dividing by zero, a value too large to keep, and what has no
  value (√−1, ln 0, tan 90°, 2.5!) are said under the formula after `=`.

## How it is made

| File | What it is |
| --- | --- |
| `src/number.cc` | `calculator.number`: exact numbers. `natural`, unbounded (32-bit limbs, Knuth's long division), and `rational`, kept in lowest terms. |
| `src/engine.cc` | `calculator.engine`: the calculator's state, one type per key, the operations as a `std::variant` dispatched by overloading, and how numbers are written (every shown digit correct). The arithmetic throws `division_by_zero` and `out_of_range`; the keys that compute catch them into the calculator's error state. Plain C++, no UI. |
| `src/page.cc` | `calculator.page`: the page, a skiff.app tree. The calculator is its one part of the model; each button sends its key, and the key's handler presses it. Also the stylesheet. |
| `src/main.cc` | The program: `SKIFF_APP(calculator::page, calculator::Hooks)`. |
| `web/index.html` | The page Emscripten fills in; its script maps the keyboard onto the buttons. |
| `test/engine.cc` | The engine's tests (googletest): key sequences, what the display shows, and what the arithmetic throws. |
| `test/page_written.cmake` | The page as the native program writes it: the display and every key. |
| `test/browser.mjs` | The published page in Chromium and Firefox, pressed by mouse and keyboard. |

## Building

Requires Clang 23, CMake 4.3.4 or newer, and Ninja. skiff-framework and its
dependencies are fetched by
[cmake-everywhere](https://github.com/j4niwzis/cmake-everywhere).

The engine and its tests alone (googletest is the only thing fetched):

```sh
cmake -S . -B build/engine -G Ninja -DCALCULATOR_ENGINE_ONLY=ON
cmake --build build/engine
(cd build/engine && ctest --output-on-failure)
```

The page, under Emscripten (see `.github/workflows/ci.yml` for the exact
toolchain), then the native program that writes the page into it:

```sh
emcmake cmake -S . -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-fexceptions -mmutable-globals"
cmake --build build/wasm --target calculator
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target calculator
mkdir -p public && cp build/wasm/index.{html,js,wasm} public/
build/native/calculator public/static.html public/index.html
```

`public/` is then a static site. CI does the same on every push, checks the
page in real browsers, and publishes `main` to GitHub Pages.

## License

AGPL-3.0-only.
