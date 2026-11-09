# Calculator

A calculator for the browser, written in C++26 with
[skiff-framework](https://github.com/j4niwzis/skiff-framework). The page is
written out when the program is built, so it shows at once; the WebAssembly
then takes it over.

## Using it

| Key | Mouse | Keyboard |
| --- | --- | --- |
| Digits, decimal point | `0`–`9`, `.` | `0`–`9`, `.` or `,` |
| Add, subtract, multiply, divide | `+` `−` `×` `÷` | `+` `-` `*` (or `x`) `/` |
| Work it out | `=` | `Enter` or `=` |
| Percent | `%` | `%` |
| Change sign | `±` | `F9` |
| Delete the last digit | `⌫` | `Backspace` |
| Clear | `C` | `Escape` or `Delete` |

How it works things out:

- **Left to right, as typed**: `2 + 3 × 4 =` is 20, as on a pocket calculator.
- **`=` again repeats the last operation**: `2 + 3 = =` is 8. `5 + =` is 10.
- **Percent** takes a percentage of the left side beside `+` and `−`
  (`200 + 5 % =` is 210) and is a hundredth beside `×` and `÷`
  (`50 × 10 % =` is 5).
- **Choosing another operation** before typing the right side replaces the
  first one: `9 + − 4 =` is 5.
- **Exact.** Every number is a rational of unbounded integers, not a
  floating-point approximation, and every key keeps it one, so every result
  is exact: `0.1 + 0.2` is `0.3`, `10^100 + 1 − 10^100` is `1`, and
  `1 ÷ 3 × 3` is `1`. (This is the rational part of Hans Boehm's arithmetic
  for Android's calculator.)
- **Every digit shown is correct.** A result is shown with up to 20
  significant digits. If it goes on (`1 ÷ 3`), its digits are cut, never
  rounded, and followed by `…`: `0.33333333333333333333…`. The value kept
  is still exact, so `1 ÷ 3 = × 3 =` is `1`. Very large or small results use
  an exponent (`1e30`, `1.2345678901234567890…e26`, `1e-30`).
- **Entries** take up to 20 digits. The display groups them by thousands.
- **Errors**: dividing by zero, and a result too large to keep (over 2^17
  bits, about 39,000 digits), are shown in place of the number. Operations
  are refused until a new number is typed or `C` is pressed.

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
