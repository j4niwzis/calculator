// SPDX-License-Identifier: AGPL-3.0-only
// The calculator itself: what it holds, what each key does to it, and how
// its numbers are written -- plain C++, no UI. The page (calculator.page)
// shows a calc and sends it keys; the tests (test/engine.cc) press the keys
// themselves.
//
// Exact: every number is a rational (calculator.number), and every key
// keeps it one, so every answer is exact; only how many of its digits are
// shown is limited, and each digit shown is right -- one that goes on is
// cut, never rounded, and followed by an ellipsis.
export module calculator.engine;

import std;
export import calculator.number;

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
// A result too large to keep: its numerator and denominator together over
// kMaxBits (repeated squaring gets there in a few presses).
struct out_of_range : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Out of range"; }
  friend bool operator==(const out_of_range &, const out_of_range &) { return true; }
};
using failure = std::variant<division_by_zero, out_of_range>;

[[nodiscard]] inline std::string_view message(const failure &what) {
  return std::visit([](const auto &one) { return std::string_view(one.what()); }, what);
}

// The most bits a result keeps: some 39,000 decimal digits.
inline constexpr std::size_t kMaxBits = std::size_t{1} << 17;

// A result as kept: one not too large.
[[nodiscard]] inline rational checked(rational v) {
  if (v.bits() > kMaxBits) throw out_of_range{};
  return v;
}
[[nodiscard]] inline rational apply(op::add, const rational &a, const rational &b) { return checked(a + b); }
[[nodiscard]] inline rational apply(op::subtract, const rational &a, const rational &b) { return checked(a - b); }
[[nodiscard]] inline rational apply(op::multiply, const rational &a, const rational &b) { return checked(a * b); }
[[nodiscard]] inline rational apply(op::divide, const rational &a, const rational &b) {
  if (b.zero()) throw division_by_zero{};
  return checked(a / b);
}
[[nodiscard]] inline rational apply(const operation &which, const rational &a, const rational &b) {
  return std::visit([&](auto one) { return apply(one, a, b); }, which);
}

// What `%` makes of the number typed, by the operation it is the right side
// of: beside + or -, that percent of the left side (200 + 5% is 200 + 10);
// beside * or /, a hundredth of it (200 * 5% is 200 * 0.05).
[[nodiscard]] inline rational percent_of(op::add, const rational &left, const rational &typed) { return left * typed / rational(100); }
[[nodiscard]] inline rational percent_of(op::subtract, const rational &left, const rational &typed) { return left * typed / rational(100); }
[[nodiscard]] inline rational percent_of(op::multiply, const rational &, const rational &typed) { return typed / rational(100); }
[[nodiscard]] inline rational percent_of(op::divide, const rational &, const rational &typed) { return typed / rational(100); }

// ---- numbers as written -----------------------------------------------------

// The most digits an entry takes, and the most significant digits a result
// is shown with.
inline constexpr std::ptrdiff_t kMaxDigits = 20;
inline constexpr std::size_t kShownDigits = 20;
// Below 1, the most zeros after the point before an exponent is used.
inline constexpr std::size_t kMaxLeadingZeros = 9;
// After a digit that goes on: there are more, none of them shown.
inline constexpr std::string_view kMore = "…";

[[nodiscard]] inline std::ptrdiff_t digits_in(std::string_view entry) {
  return std::ranges::count_if(entry, [](char c) { return c >= '0' && c <= '9'; });
}

// Digits as a mantissa: the first, a point, the rest without the zeros they
// end in (where nothing comes after them).
[[nodiscard]] inline std::string mantissa(std::string digits, bool exact) {
  if (exact) {
    const std::size_t last = digits.find_last_not_of('0');
    digits.resize(last == std::string::npos ? 1 : last + 1);
  }
  return digits.size() == 1 ? digits : std::format("{}.{}", digits.front(), std::string_view(digits).substr(1));
}

// A result as shown: exact where it ends within kShownDigits significant
// digits (0.3, 1,234.5); else its first kShownDigits digits, cut, and an
// ellipsis (0.33333333333333333333…). An exponent where the whole part
// has more digits than that (1e30, 1.2345…e40), or the number is below
// 10^-kMaxLeadingZeros (1e-30).
[[nodiscard]] inline std::string written(const rational &q) {
  if (q.zero()) return "0";
  const std::string_view sign = q.negative() ? "-" : "";
  const natural &den = q.denominator();
  auto [whole, rest] = divmod(q.numerator(), den);
  // The fraction's next digit.
  const auto next = [&] {
    rest.multiply_add(10, 0);
    auto [d, r] = divmod(rest, den);
    rest = std::move(r);
    return static_cast<char>('0' + d.low());
  };
  if (!whole.zero()) {
    const std::string digits = whole.decimal();
    if (digits.size() > kShownDigits) {
      const bool exact = rest.zero() && digits.find_first_not_of('0', kShownDigits) == std::string::npos;
      return std::format("{}{}{}e{}", sign, mantissa(digits.substr(0, kShownDigits), exact), exact ? "" : kMore, digits.size() - 1);
    }
    std::string fraction;
    while (digits.size() + fraction.size() < kShownDigits && !rest.zero()) fraction += next();
    const bool exact = rest.zero();
    return std::format("{}{}{}{}{}", sign, digits, fraction.empty() ? "" : ".", fraction, exact ? "" : kMore);
  }
  // Below 1: the zeros after the point, then the significant digits.
  std::size_t zeros = 0;
  char first = next();
  for (; first == '0'; first = next()) ++zeros;
  std::string significant(1, first);
  while (significant.size() < kShownDigits && !rest.zero()) significant += next();
  const bool exact = rest.zero();
  if (zeros > kMaxLeadingZeros)
    return std::format("{}{}{}e-{}", sign, mantissa(significant, exact), exact ? "" : kMore, zeros + 1);
  return std::format("{}0.{}{}{}", sign, std::string(zeros, '0'), significant, exact ? "" : kMore);
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
// A number as shown: its whole part grouped by thousands, the rest (the
// fraction, an ellipsis, an exponent) as it is.
[[nodiscard]] inline std::string grouped(std::string_view entry) {
  const bool negative = entry.starts_with('-');
  const std::string_view body = entry.substr(negative ? 1 : 0);
  const std::size_t whole = std::min(body.find_first_not_of("0123456789"), body.size());
  return std::format("{}{}{}", negative ? "-" : "", grouped_digits(body.substr(0, whole)), body.substr(whole));
}

// ---- the calculator ---------------------------------------------------------

struct calc {
  // The number shown: as typed, or the last result as written().
  std::string entry = "0";
  // Its value, exactly (a result shown cut is kept whole here).
  rational value;
  // The left side and the operation waiting for the right side.
  std::optional<rational> left;
  std::optional<operation> pending;
  // What `=` did last, and its right side: `=` again does it again.
  std::optional<operation> repeat_op;
  rational repeat_right;
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
// A result shown, and kept exactly.
inline void show(calc &c, rational result) {
  c.entry = written(result);
  c.value = std::move(result);
}
// The entry edited by typing: its value read again.
inline void typed(calc &c, std::string entry) {
  c.value = rational::from_decimal(entry);
  c.entry = std::move(entry);
}

[[nodiscard]] inline calc press(calc c, digit d) {
  if (c.failed) c = calc{};
  if (c.fresh && !c.pending) c.expression.clear();
  const char typed_now = static_cast<char>('0' + std::clamp(d.value, 0, 9));
  if (c.fresh) {
    typed(c, std::string(1, typed_now));
    c.fresh = false;
  } else if (c.entry == "0") {
    typed(c, std::string(1, typed_now));
  } else if (c.entry == "-0") {
    typed(c, std::string{'-', typed_now});
  } else if (digits_in(c.entry) < kMaxDigits) {
    typed(c, c.entry + typed_now);
  }
  c.given = true;
  return c;
}

[[nodiscard]] inline calc press(calc c, point) {
  if (c.failed) c = calc{};
  if (c.fresh && !c.pending) c.expression.clear();
  if (c.fresh) {
    typed(c, "0.");
    c.fresh = false;
  } else if (!c.entry.contains('.')) {
    typed(c, c.entry + '.');
  }
  c.given = true;
  return c;
}

// An operation chosen: what was waiting worked out first (it may throw).
[[nodiscard]] inline calc chosen_on(calc c, choose chosen) {
  if (c.pending && c.given) {
    rational result = apply(*c.pending, *c.left, c.value);
    c.left = result;
    show(c, std::move(result));
  } else if (!c.pending) {
    c.left = c.value;
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
  rational left;
  if (c.pending) {
    left = *c.left;
    c.repeat_op = c.pending;
    c.repeat_right = c.given ? c.value : left;
  } else if (c.repeat_op) {
    left = c.value;
  } else {
    c.fresh = true;
    return c;
  }
  rational result = apply(*c.repeat_op, left, c.repeat_right);
  c.expression = std::format("{} {} {} =", grouped(written(left)), symbol(*c.repeat_op), grouped(written(c.repeat_right)));
  show(c, std::move(result));
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
  typed(c, "0");
  c.fresh = true;
  c.given = c.pending.has_value();
  return c;
}

[[nodiscard]] inline calc press(calc c, backspace) {
  if (c.failed) return calc{};
  if (c.fresh) return c;
  std::string entry = c.entry;
  entry.pop_back();
  typed(c, entry.empty() || entry == "-" ? std::string("0") : std::move(entry));
  return c;
}

[[nodiscard]] inline calc press(calc c, negate) {
  if (c.failed) return c;
  if (c.pending && !c.given) {
    typed(c, "-0");
    c.fresh = false;
    c.given = true;
    return c;
  }
  if (c.entry.starts_with('-'))
    c.entry.erase(0, 1);
  else
    c.entry.insert(0, 1, '-');
  c.value = -c.value;
  if (c.entry == "-0") c.fresh = false;
  if (c.pending) c.given = true;
  return c;
}

[[nodiscard]] inline calc press(calc c, percent) {
  if (c.failed) return c;
  return guarded([&] {
    rational made = c.pending ? std::visit([&](auto one) { return percent_of(one, *c.left, c.value); }, *c.pending)
                              : c.value / rational(100);
    show(c, checked(std::move(made)));
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
