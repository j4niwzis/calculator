// SPDX-License-Identifier: AGPL-3.0-only
// The calculator as a page: the display over the keys, the calculator
// itself (calculator.engine) its one part of the model. A key sends itself;
// its handler presses it on the calculator. The keyboard presses the same
// buttons (web/index.html).
module;
// Named views: skiff.ui says what they are over (SKIFF_NAMED_BASE, ui::Named).
#include <skiff/named.h>
export module calculator.page;

import std;
import skiff.app;
import calculator.engine;

namespace calculator {
using namespace skiff;
using namespace skiff::ui;
}  // namespace calculator

export namespace calculator {

namespace colour {
inline constexpr scene::Colour kBody = 0xff1b232bu;
inline constexpr scene::Colour kInk = 0xffe8eef2u;
inline constexpr scene::Colour kDim = 0xff8a9aa6u;
}  // namespace colour

// A key's handler: the key pressed on the calculator.
template <class Key> auto on_key() {
  return handle<Key>([](const Key &key) { return model::over<calc>(pressed<Key>{key}); });
}
// A key's button: its label, and the key it sends.
template <class Key> auto key_button(std::string label, Key key) {
  return SendButton<Key>(std::move(label), std::move(key), {.height = 64.0f, .grow = scene::axes::kX});
}

// The display: what is being worked out, over the number.
inline auto display_lines() {
  return SKIFF_NAMED(Display,
                     column(vbox(4.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = {12.0f, 8.0f, 12.0f, 8.0f}}),
                            styled({.minHeight = 22.0f, .alignSelf = scene::align::kEnd},
                                   text_for<calc>([](const calc &c) { return expression(c); }, Text(std::string(), 18.0f, colour::kDim))),
                            styled({.minHeight = 52.0f, .alignSelf = scene::align::kEnd},
                                   text_for<calc>([](const calc &c) { return display(c); }, Text("0", 44.0f, colour::kInk, true)))));
}

// A row of keys. Not named (SKIFF_NAMED): a named view between a key and
// the state whose handlers take it keeps the key's event from reaching
// them.
template <class... Keys> auto key_row(Keys... keys) {
  return row(hbox(10.0f, {.fillX = true, .autoSize = scene::axes::kY}), std::move(keys)...);
}

// The calculator, named on the page around the state that keeps it.
auto page() {
  return SKIFF_NAMED(Calculator,
      state<calc>(
          handlers(on_key<digit>(), on_key<point>(), on_key<choose>(), on_key<equals>(), on_key<clear>(), on_key<backspace>(),
                   on_key<negate>(), on_key<percent>()),
                  column(vbox(10.0f, {.fillX = true, .autoSize = scene::axes::kY, .maxWidth = 380.0f,
                                      .padding = {16.0f, 16.0f, 16.0f, 16.0f}, .cornerRadius = 20.0f,
                                      .alignSelf = scene::align::kMiddle, .background = colour::kBody}),
                         display_lines(),
                         key_row(key_button("C", clear{}), key_button("⌫", backspace{}), key_button("%", percent{}),
                                      key_button("÷", choose{op::divide{}})),
                         key_row(key_button("7", digit{7}), key_button("8", digit{8}), key_button("9", digit{9}),
                                 key_button("×", choose{op::multiply{}})),
                         key_row(key_button("4", digit{4}), key_button("5", digit{5}), key_button("6", digit{6}),
                                 key_button("−", choose{op::subtract{}})),
                         key_row(key_button("1", digit{1}), key_button("2", digit{2}), key_button("3", digit{3}),
                                 key_button("+", choose{op::add{}})),
                         key_row(key_button("±", negate{}), key_button("0", digit{0}), key_button(".", point{}),
                                 key_button("=", equals{})))));
}

using Model = skiff::ui::ModelOf<decltype(page())>;

// The page's look: dark, the keys round, the operations in amber; the
// display's number broken where it is longer than the calculator is wide.
inline constexpr std::string_view kCss = R"(
html,body{margin:0;min-height:100%;background:#0f1419}
#skiff{font:16px/1.3 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;color:#e8eef2;min-height:100vh;display:flex;flex-direction:column;justify-content:center;padding:16px;box-sizing:border-box}
[data-view=Calculator]{width:100%;max-width:380px;margin:0 auto;border-radius:20px;box-shadow:0 20px 50px rgba(0,0,0,.45)}
[data-view=Display]>*{text-align:right;word-break:break-all;max-width:100%}
[data-view=Calculator] .skiff-button{flex:1 1 0;min-width:0;height:64px;border:0!important;border-radius:14px!important;font:500 24px/1 inherit!important;background:#2a3540!important;color:#e8eef2!important;cursor:pointer;user-select:none;-webkit-tap-highlight-color:transparent;transition:filter .08s}
[data-view=Calculator] .skiff-button:last-child{background:#e18a32!important;color:#fff!important}
.skiff-button:hover{filter:brightness(1.15)}
.skiff-button:active,.skiff-button.pressed{filter:brightness(.85)}
.skiff-button:focus-visible{outline:2px solid #8fd3e0;outline-offset:2px}
)";

// What the calculator adds to skiff.app's program (main.cc): its look and
// what is written into the head of the page.
struct Hooks {
  [[nodiscard]] std::string_view stylesheet() const { return kCss; }
  [[nodiscard]] std::string_view head() const {
    return "<title>Calculator</title>\n"
           "<meta name=\"description\" content=\"A calculator, written in C++ with skiff.\">\n";
  }
};

}  // namespace calculator
