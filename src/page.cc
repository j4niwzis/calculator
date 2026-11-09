// SPDX-License-Identifier: AGPL-3.0-only
// The calculator, laid out as Android's: the formula over its result, the
// scientific keys (folded to one row, opened to three), the keypad filling
// the rest. Every look is said here, in the nodes' specs and the keys'
// themes -- one tree, drawn the same by the page and by Skia in a window.
// The layout is skiff's: the keypad's rows and keys grow to fill whatever
// the window is.
module;
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

// Material You's dark scheme, as Android's calculator wears it.
namespace colour {
inline constexpr scene::Colour kBackground = 0xff1b1b1fu;
inline constexpr scene::Colour kFormula = 0xffe4e1e6u;
inline constexpr scene::Colour kResult = 0xff938f99u;
inline constexpr scene::Colour kNumber = 0xff303034u;
inline constexpr scene::Colour kOnNumber = 0xffe4e1e6u;
inline constexpr scene::Colour kOperator = 0xff4a4458u;
inline constexpr scene::Colour kOnOperator = 0xffe8def8u;
inline constexpr scene::Colour kClear = 0xff633b48u;
inline constexpr scene::Colour kOnClear = 0xffffd8e4u;
inline constexpr scene::Colour kEquals = 0xffd0bcffu;
inline constexpr scene::Colour kOnEquals = 0xff381e72u;
inline constexpr scene::Colour kScientific = 0xff1b1b1fu;
inline constexpr scene::Colour kOnScientific = 0xffcac4d0u;
}  // namespace colour

// A key's look: its surface and text, its corners round, its label's size.
[[nodiscard]] inline Theme key_theme(scene::Colour surface, scene::Colour text, float size) {
  Theme t = theme();
  t.fSurface = surface;
  t.fSurfaceHover = surface;
  t.fSurfaceActive = surface;
  t.fText = text;
  t.fLabel = text;
  t.fCorner = 999.0f;  // as round as the key is short: a pill, a circle where it is square
  t.fFontSize = size;
  return t;
}
inline const Theme kNumberKey = key_theme(colour::kNumber, colour::kOnNumber, 28.0f);
inline const Theme kOperatorKey = key_theme(colour::kOperator, colour::kOnOperator, 28.0f);
inline const Theme kClearKey = key_theme(colour::kClear, colour::kOnClear, 24.0f);
inline const Theme kEqualsKey = key_theme(colour::kEquals, colour::kOnEquals, 30.0f);
inline const Theme kScientificKey = key_theme(colour::kScientific, colour::kOnScientific, 18.0f);

// A key's handler: the key pressed on the calculator.
template <class Key> auto on_key() {
  return handle<Key>([](const Key &key) { return model::over<calc>(pressed<Key>{key}); });
}
// A key, growing to fill its share of its row and its row's height.
template <class Key> auto key(std::string label, Key k, const Theme &look) {
  return SendButton<Key>(std::move(label), std::move(k), {.fillY = true, .grow = scene::axes::kX}, look);
}
// A key whose label and function follow the inverse key: sin and sin⁻¹.
template <class F> auto inverting(F of_calc, std::string label, of first) {
  return projected<calc>(std::move(of_calc), key(std::move(label), first, kScientificKey));
}
struct sin_or_inverse {
  std::pair<std::string, of> operator()(const calc &c) const {
    return c.inverse ? std::pair{std::string("sin\u207b\u00b9"), of{fn::arcsine{}}} : std::pair{std::string("sin"), of{fn::sine{}}};
  }
};
struct cos_or_inverse {
  std::pair<std::string, of> operator()(const calc &c) const {
    return c.inverse ? std::pair{std::string("cos\u207b\u00b9"), of{fn::arccosine{}}} : std::pair{std::string("cos"), of{fn::cosine{}}};
  }
};
struct tan_or_inverse {
  std::pair<std::string, of> operator()(const calc &c) const {
    return c.inverse ? std::pair{std::string("tan\u207b\u00b9"), of{fn::arctangent{}}} : std::pair{std::string("tan"), of{fn::tangent{}}};
  }
};
struct ln_or_inverse {
  std::pair<std::string, of> operator()(const calc &c) const {
    return c.inverse ? std::pair{std::string("e\u02e3"), of{fn::exponential{}}} : std::pair{std::string("ln"), of{fn::natural_log{}}};
  }
};
struct log_or_inverse {
  std::pair<std::string, of> operator()(const calc &c) const {
    return c.inverse ? std::pair{std::string("10\u02e3"), of{fn::ten_to{}}} : std::pair{std::string("log"), of{fn::common_log{}}};
  }
};
struct angles_label {
  std::pair<std::string, switch_angles> operator()(const calc &c) const {
    return {std::string(angles(c)), switch_angles{}};
  }
};

// A row of keys, a share of the height its column has.
template <class... Keys> auto key_row(Keys... keys) {
  return row(hbox(8.0f, {.fillX = true, .grow = scene::axes::kY}), std::move(keys)...);
}

// The display: the formula, large, over its result; the angles read.
inline auto display_lines() {
  return SKIFF_NAMED(Display,
                     column(vbox(8.0f, {.fillX = true, .grow = scene::axes::kY, .padding = {24.0f, 24.0f, 12.0f, 24.0f}}),
                            styled({.alignSelf = scene::align::kStart},
                                   text_for<calc>([](const calc &c) { return std::string(angles(c)); }, Text("RAD", 14.0f, colour::kResult))),
                            styled({.grow = scene::axes::kY}, column(vbox(0.0f, {.fillX = true}))),
                            styled({.minHeight = 64.0f, .alignSelf = scene::align::kEnd},
                                   text_for<calc>([](const calc &c) { return formula_text(c); }, Text(std::string(), 52.0f, colour::kFormula))),
                            styled({.minHeight = 36.0f, .alignSelf = scene::align::kEnd},
                                   text_for<calc>([](const calc &c) { return result_text(c); }, Text(std::string(), 30.0f, colour::kResult)))));
}

// The scientific keys: one row folded, three opened (the chevron says
// which), as Android has them in a portrait window.
inline auto scientific_folded() {
  return column(vbox(4.0f, {.fillX = true, .height = 52.0f}),
                key_row(key("\u221a", of{fn::square_root{}}, kScientificKey), key("\u03c0", put{constant::pi{}}, kScientificKey),
                        key("^", choose{op::power{}}, kScientificKey), key("!", after{post::factorial{}}, kScientificKey),
                        key("\u2304", expand{}, kScientificKey)));
}
inline auto scientific_opened() {
  return column(vbox(4.0f, {.fillX = true, .height = 156.0f}),
                key_row(key("\u221a", of{fn::square_root{}}, kScientificKey), key("\u03c0", put{constant::pi{}}, kScientificKey),
                        key("^", choose{op::power{}}, kScientificKey), key("!", after{post::factorial{}}, kScientificKey),
                        key("\u2303", expand{}, kScientificKey)),
                key_row(projected<calc>(angles_label{}, key("RAD", switch_angles{}, kScientificKey)),
                        inverting(sin_or_inverse{}, "sin", of{fn::sine{}}), inverting(cos_or_inverse{}, "cos", of{fn::cosine{}}),
                        inverting(tan_or_inverse{}, "tan", of{fn::tangent{}}), key("e", put{constant::e{}}, kScientificKey)),
                key_row(key("INV", switch_inverse{}, kScientificKey), inverting(ln_or_inverse{}, "ln", of{fn::natural_log{}}),
                        inverting(log_or_inverse{}, "log", of{fn::common_log{}}), key("%", after{post::percent{}}, kScientificKey),
                        key("x\u00b2", after{post::squared{}}, kScientificKey)));
}

// The keypad: Android's five rows of four.
inline auto keypad() {
  return column(vbox(8.0f, {.fillX = true, .grow = scene::axes::kY, .padding = {8.0f, 16.0f, 16.0f, 16.0f}}),
                key_row(key("AC", clear{}, kClearKey), key("( )", parentheses{}, kOperatorKey), key("%", after{post::percent{}}, kOperatorKey),
                        key("\u00f7", choose{op::divide{}}, kOperatorKey)),
                key_row(key("7", digit{7}, kNumberKey), key("8", digit{8}, kNumberKey), key("9", digit{9}, kNumberKey),
                        key("\u00d7", choose{op::multiply{}}, kOperatorKey)),
                key_row(key("4", digit{4}, kNumberKey), key("5", digit{5}, kNumberKey), key("6", digit{6}, kNumberKey),
                        key("\u2212", choose{op::subtract{}}, kOperatorKey)),
                key_row(key("1", digit{1}, kNumberKey), key("2", digit{2}, kNumberKey), key("3", digit{3}, kNumberKey),
                        key("+", choose{op::add{}}, kOperatorKey)),
                key_row(key("0", digit{0}, kNumberKey), key(".", point{}, kNumberKey), key("\u232b", backspace{}, kNumberKey),
                        key("=", equals{}, kEqualsKey)));
}

auto page() {
  return SKIFF_NAMED(Calculator,
      state<calc>(
          handlers(on_key<digit>(), on_key<point>(), on_key<choose>(), on_key<put>(), on_key<of>(), on_key<after>(),
                   on_key<parentheses>(), on_key<equals>(), on_key<clear>(), on_key<backspace>(), on_key<switch_angles>(),
                   on_key<switch_inverse>(), on_key<expand>()),
          column(vbox(0.0f, {.fill = true, .background = colour::kBackground}),
                 display_lines(),
                 switch_on<calc>([](const calc &c) { return c.expanded ? std::size_t{1} : std::size_t{0}; }, scientific_folded(),
                                 scientific_opened()),
                 styled({.fillX = true, .grow = scene::axes::kY}, keypad()))));
}

using Model = skiff::ui::ModelOf<decltype(page())>;

// What the calculator adds to skiff.app's program (main.cc): the paper
// behind it, and what is written into the head of the page. Its look is
// the tree's, above.
struct Hooks {
  [[nodiscard]] scene::Colour paper() const { return colour::kBackground; }
  [[nodiscard]] std::string_view stylesheet() const { return "html,body{margin:0;height:100%}#skiff{height:100%}"; }
  [[nodiscard]] std::string_view head() const {
    return "<title>Calculator</title>\n"
           "<meta name=\"description\" content=\"A calculator, written in C++ with skiff.\">\n";
  }
};

}  // namespace calculator
