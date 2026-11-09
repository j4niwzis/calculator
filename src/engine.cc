// SPDX-License-Identifier: AGPL-3.0-only
// The calculator itself: what it holds, what each key does to it, and how
// its numbers are written -- plain C++, no UI. The page (calculator.page)
// shows a calc and sends it keys; the tests (test/engine.cc) press the keys
// themselves.
export module calculator.engine;

import std;

export namespace calculator {

// ---- the operations ---------------------------------------------------------

namespace op {
struct add {
  friend bool operator==(add, add) = default;
};
struct subtract {
  friend bool operator==(subtract, subtract) = default;
};
struct multiply {
  friend bool operator==(multiply, multiply) = default;
};
struct divide {
  friend bool operator==(divide, divide) = default;
};
}  // namespace op
using operation = std::variant<op::add, op::subtract, op::multiply, op::divide>;

// What an operation is written as.
[[nodiscard]] constexpr std::string_view symbol(op::add) { return "+"; }
[[nodiscard]] constexpr std::string_view symbol(op::subtract) { return "−"; }
[[nodiscard]] constexpr std::string_view symbol(op::multiply) { return "×"; }
[[nodiscard]] constexpr std::string_view symbol(op::divide) { return "÷"; }
[[nodiscard]] constexpr std::string_view symbol(const operation &which) {
  return std::visit([](auto one) { return symbol(one); }, which);
}

// ---- what can go wrong ------------------------------------------------------

// Thrown by the arithmetic; each says what it is (what()). The keys that
// work something out catch them, and the calculator keeps the one caught
// as its state (calc::failed) until a new number or C.
struct division_by_zero : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Cannot divide by zero"; }
  friend bool operator==(const division_by_zero &, const division_by_zero &) { return true; }
};
// A result too large for a double (or no number at all).
struct out_of_range : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Out of range"; }
  friend bool operator==(const out_of_range &, const out_of_range &) { return true; }
};
using failure = std::variant<division_by_zero, out_of_range>;

[[nodiscard]] inline std::string_view message(const failure &what) {
  return std::visit([](const auto &one) { return std::string_view(one.what()); }, what);
}

// A result as kept: a finite number, never a negative zero.
[[nodiscard]] inline double checked(double v) {
  if (!std::isfinite(v)) throw out_of_range{};
  return v == 0.0 ? 0.0 : v;
}
[[nodiscard]] inline double apply(op::add, double a, double b) { return checked(a + b); }
[[nodiscard]] inline double apply(op::subtract, double a, double b) { return checked(a - b); }
[[nodiscard]] inline double apply(op::multiply, double a, double b) { return checked(a * b); }
[[nodiscard]] inline double apply(op::divide, double a, double b) {
  if (b == 0.0) throw division_by_zero{};
  return checked(a / b);
}
[[nodiscard]] inline double apply(const operation &which, double a, double b) {
  return std::visit([&](auto one) { return apply(one, a, b); }, which);
}

// What `%` makes of the number typed, by the operation it is the right side
// of: beside + or -, that percent of the left side (200 + 5% is 200 + 10);
// beside * or /, a hundredth of it (200 * 5% is 200 * 0.05).
[[nodiscard]] inline double percent_of(op::add, double left, double typed) { return left * typed / 100.0; }
[[nodiscard]] inline double percent_of(op::subtract, double left, double typed) { return left * typed / 100.0; }
[[nodiscard]] inline double percent_of(op::multiply, double, double typed) { return typed / 100.0; }
[[nodiscard]] inline double percent_of(op::divide, double, double typed) { return typed / 100.0; }

// ---- numbers as written -----------------------------------------------------

// The most digits an entry takes.
inline constexpr std::ptrdiff_t kMaxDigits = 15;

// An entry's number. An entry is only ever made here, so it always reads.
[[nodiscard]] inline double value_of(std::string_view entry) {
  double v = 0.0;
  std::from_chars(entry.data(), entry.data() + entry.size(), v);
  return v;
}
[[nodiscard]] inline std::ptrdiff_t digits_in(std::string_view entry) {
  return std::ranges::count_if(entry, [](char c) { return c >= '0' && c <= '9'; });
}

// A result as an entry: 12 significant digits, what a double holds of the
// decimal it was meant to be (0.1 + 0.2 is 0.3); an exponent where the
// number is too large or small for them, written 1e16 and 1e-7.
[[nodiscard]] inline std::string written(double v) {
  const std::string text = std::format("{:.12g}", v);
  const std::size_t e = text.find('e');
  if (e == std::string::npos) return text;
  const std::size_t from = e + 1 + (text[e + 1] == '+' ? 1 : 0);
  int exponent = 0;
  std::from_chars(text.data() + from, text.data() + text.size(), exponent);
  return std::format("{}e{}", std::string_view(text).substr(0, e), exponent);
}

// Digits grouped by thousands from the right: 1234567 -> 1,234,567.
[[nodiscard]] inline std::string grouped_digits(std::string_view digits) {
  // From the right: every third digit after the first has a comma before it
  // (in reverse order, after it).
  std::string out = digits | std::views::reverse | std::views::enumerate | std::views::transform([](auto at) {
                      const auto [i, c] = at;
                      return i > 0 && i % 3 == 0 ? std::string{',', c} : std::string(1, c);
                    }) |
                    std::views::join | std::ranges::to<std::string>();
  std::ranges::reverse(out);
  return out;
}
// An entry as shown: its whole part grouped by thousands, the rest (the
// fraction, an exponent) as typed.
[[nodiscard]] inline std::string grouped(std::string_view entry) {
  const bool negative = entry.starts_with('-');
  const std::string_view body = entry.substr(negative ? 1 : 0);
  const std::size_t whole = std::min(body.find_first_of(".e"), body.size());
  return std::format("{}{}{}", negative ? "-" : "", grouped_digits(body.substr(0, whole)), body.substr(whole));
}

// ---- the calculator ---------------------------------------------------------

struct calc {
  // The number typed, or the last result: as an entry (written()).
  std::string entry = "0";
  // The left side and the operation waiting for the right side.
  std::optional<double> left;
  std::optional<operation> pending;
  // What `=` did last, and its right side: `=` again does it again.
  std::optional<operation> repeat_op;
  double repeat_right = 0.0;
  // The next digit starts a new entry (a result, or nothing typed yet).
  bool fresh = true;
  // A right side was given since the operation was chosen.
  bool given = false;
  // What is being worked out, as shown over the entry: "12 +", "12 + 3 =".
  std::string expression;
  // Set: the last operation went wrong. Operations are refused until a
  // number is typed or C starts again.
  std::optional<failure> failed;
  friend bool operator==(const calc &, const calc &) = default;
};

// ---- the keys ---------------------------------------------------------------

struct digit {
  int value = 0;  // 0 to 9
};
struct point {};
struct choose {
  operation which;
};
struct equals {};
struct clear {};
struct clear_entry {};
struct backspace {};
struct negate {};
struct percent {};

// What went wrong, shown in place of the entry.
[[nodiscard]] inline calc failing(failure what) {
  calc out;
  out.failed = std::move(what);
  return out;
}
// A key that works something out: what its arithmetic throws caught here,
// and kept as the calculator's state.
template <class Work> [[nodiscard]] calc guarded(Work work) {
  try {
    return work();
  } catch (const division_by_zero &caught) {
    return failing(caught);
  } catch (const out_of_range &caught) {
    return failing(caught);
  }
}

[[nodiscard]] inline calc press(calc c, digit d) {
  if (c.failed) c = calc{};
  if (c.fresh && !c.pending) c.expression.clear();
  const char typed = static_cast<char>('0' + std::clamp(d.value, 0, 9));
  if (c.fresh) {
    c.entry = std::string(1, typed);
    c.fresh = false;
  } else if (c.entry == "0") {
    c.entry = std::string(1, typed);
  } else if (c.entry == "-0") {
    c.entry = std::string{'-', typed};
  } else if (digits_in(c.entry) < kMaxDigits) {
    c.entry += typed;
  }
  c.given = true;
  return c;
}

[[nodiscard]] inline calc press(calc c, point) {
  if (c.failed) c = calc{};
  if (c.fresh && !c.pending) c.expression.clear();
  if (c.fresh) {
    c.entry = "0.";
    c.fresh = false;
  } else if (!c.entry.contains('.')) {
    c.entry += '.';
  }
  c.given = true;
  return c;
}

// An operation chosen: what was waiting worked out first (it may throw).
[[nodiscard]] inline calc chosen_on(calc c, choose chosen) {
  if (c.pending && c.given) {
    const double result = apply(*c.pending, *c.left, value_of(c.entry));
    c.left = result;
    c.entry = written(result);
  } else if (!c.pending) {
    c.left = value_of(c.entry);
  }
  c.pending = chosen.which;
  c.expression = std::format("{} {}", grouped(written(*c.left)), symbol(chosen.which));
  c.fresh = true;
  c.given = false;
  c.repeat_op.reset();
  return c;
}

[[nodiscard]] inline calc press(calc c, choose chosen) {
  if (c.failed) return c;
  return guarded([&] { return chosen_on(std::move(c), chosen); });
}

// `=`: the operation waiting, or the last one again (it may throw).
[[nodiscard]] inline calc equals_on(calc c) {
  double left = 0.0;
  if (c.pending) {
    left = *c.left;
    c.repeat_op = c.pending;
    c.repeat_right = c.given ? value_of(c.entry) : left;
  } else if (c.repeat_op) {
    left = value_of(c.entry);
  } else {
    c.fresh = true;
    return c;
  }
  const double result = apply(*c.repeat_op, left, c.repeat_right);
  c.expression = std::format("{} {} {} =", grouped(written(left)), symbol(*c.repeat_op), grouped(written(c.repeat_right)));
  c.entry = written(result);
  c.left.reset();
  c.pending.reset();
  c.fresh = true;
  c.given = false;
  return c;
}

[[nodiscard]] inline calc press(calc c, equals) {
  if (c.failed) return c;
  return guarded([&] { return equals_on(std::move(c)); });
}

[[nodiscard]] inline calc press(calc, clear) { return calc{}; }

[[nodiscard]] inline calc press(calc c, clear_entry) {
  if (c.failed) return calc{};
  c.entry = "0";
  c.fresh = true;
  c.given = c.pending.has_value();
  return c;
}

[[nodiscard]] inline calc press(calc c, backspace) {
  if (c.failed) return calc{};
  if (c.fresh) return c;
  c.entry.pop_back();
  if (c.entry.empty() || c.entry == "-") c.entry = "0";
  return c;
}

[[nodiscard]] inline calc press(calc c, negate) {
  if (c.failed) return c;
  if (c.pending && !c.given) {
    c.entry = "-0";
    c.fresh = false;
    c.given = true;
    return c;
  }
  if (c.entry.starts_with('-'))
    c.entry.erase(0, 1);
  else
    c.entry.insert(0, 1, '-');
  if (c.entry == "-0") c.fresh = false;
  if (c.pending) c.given = true;
  return c;
}

[[nodiscard]] inline calc press(calc c, percent) {
  if (c.failed) return c;
  return guarded([&] {
    const double typed = value_of(c.entry);
    const double made = c.pending ? std::visit([&](auto one) { return percent_of(one, *c.left, typed); }, *c.pending)
                                  : typed / 100.0;
    c.entry = written(checked(made));
    c.fresh = true;
    if (c.pending) c.given = true;
    return std::move(c);
  });
}

// A key, as an edit of the calculator the page keeps (model::over).
template <class Key> struct pressed {
  Key key;
  [[nodiscard]] calc operator()(calc c) const { return press(std::move(c), key); }
};

// ---- what is shown ----------------------------------------------------------

// The main line: the entry, grouped; or what went wrong.
[[nodiscard]] inline std::string display(const calc &c) {
  if (c.failed) return std::string(message(*c.failed));
  return grouped(c.entry);
}
// The line over it.
[[nodiscard]] inline std::string expression(const calc &c) { return c.expression; }

}  // namespace calculator
