// SPDX-License-Identifier: AGPL-3.0-only
// The calculator itself: what it holds, what each key does to it, and how
// its numbers are written -- plain C++, no UI. The page (calculator.page)
// shows a calc and sends it keys; the tests (test/engine.cc) press the keys
// themselves.
//
// Every number is kept as Boehm's calculator keeps it (calculator.symbolic):
// a rational where it is one -- every answer of + - x / % exact -- and a
// rational times pi, a root, a power of e, a logarithm or a trigonometric
// value where it is not, a constructive real where nothing else fits. What
// is shown of a number is its digits, each one right: a rational's end, or
// are cut and followed by an ellipsis; an irrational's are cut always.
export module calculator.engine;

import std;
export import calculator.symbolic;

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
struct power {
  friend bool operator==(power, power) = default;
};
}  // namespace op
using operation = std::variant<op::add, op::subtract, op::multiply, op::divide, op::power>;

// What an operation is written as.
[[nodiscard]] constexpr std::string_view symbol(op::add) { return "+"; }
[[nodiscard]] constexpr std::string_view symbol(op::subtract) { return "−"; }
[[nodiscard]] constexpr std::string_view symbol(op::multiply) { return "×"; }
[[nodiscard]] constexpr std::string_view symbol(op::divide) { return "÷"; }
[[nodiscard]] constexpr std::string_view symbol(op::power) { return "^"; }
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
// A result too large to keep: over kMaxBits written down (repeated squaring
// gets there in a few presses).
struct out_of_range : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Out of range"; }
  friend bool operator==(const out_of_range &, const out_of_range &) { return true; }
};
using failure = std::variant<division_by_zero, out_of_range, not_defined>;

[[nodiscard]] inline std::string_view message(const failure &what) {
  return std::visit([](const auto &one) { return std::string_view(one.what()); }, what);
}

// The most bits a result keeps: some 39,000 decimal digits.
inline constexpr std::size_t kMaxBits = std::size_t{1} << 17;

// A result as kept: one not too large.
[[nodiscard]] inline number checked(number v) {
  if (v.coefficient.bits() > kMaxBits) throw out_of_range{};
  return v;
}
[[nodiscard]] inline number apply(op::add, const number &a, const number &b) { return checked(a + b); }
[[nodiscard]] inline number apply(op::subtract, const number &a, const number &b) { return checked(a - b); }
[[nodiscard]] inline number apply(op::multiply, const number &a, const number &b) { return checked(a * b); }
[[nodiscard]] inline number apply(op::divide, const number &a, const number &b) {
  if (zero(b)) throw division_by_zero{};
  return checked(a / b);
}
[[nodiscard]] inline number apply(op::power, const number &a, const number &b) { return checked(pow(a, b)); }
[[nodiscard]] inline number apply(const operation &which, const number &a, const number &b) {
  return std::visit([&](auto one) { return apply(one, a, b); }, which);
}

// What `%` makes of the number typed, by the operation it is the right side
// of: beside + or -, that percent of the left side (200 + 5% is 200 + 10);
// beside the others, a hundredth of it (200 * 5% is 200 * 0.05).
[[nodiscard]] inline number hundredth(const number &typed) { return typed * number(rational(1) / rational(100)); }
[[nodiscard]] inline number percent_of(op::add, const number &left, const number &typed) { return left * hundredth(typed); }
[[nodiscard]] inline number percent_of(op::subtract, const number &left, const number &typed) { return left * hundredth(typed); }
[[nodiscard]] inline number percent_of(op::multiply, const number &, const number &typed) { return hundredth(typed); }
[[nodiscard]] inline number percent_of(op::divide, const number &, const number &typed) { return hundredth(typed); }
[[nodiscard]] inline number percent_of(op::power, const number &, const number &typed) { return hundredth(typed); }

// ---- numbers as written -----------------------------------------------------

// The most digits an entry takes, and the most significant digits a result
// is shown with (more asked for, more shown).
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

// A rational as shown: exact where it ends within `shown` significant
// digits (0.3, 1,234.5); else its first `shown` digits, cut, and an
// ellipsis (0.33333333333333333333…). An exponent where the whole part has
// more digits than that (1e30, 1.2345…e40), or the number is below
// 10^-kMaxLeadingZeros (1e-30).
[[nodiscard]] inline std::string written(const rational &q, std::size_t shown = kShownDigits) {
  if (q.zero()) return "0";
  const std::string_view sign = q.negative() ? "-" : "";
  const natural &den = q.denominator();
  auto [whole, rest] = divmod(q.numerator(), den);
  const auto next = [&] {
    rest.multiply_add(10, 0);
    auto [d, r] = divmod(rest, den);
    rest = std::move(r);
    return static_cast<char>('0' + d.low());
  };
  if (!whole.zero()) {
    const std::string digits = whole.decimal();
    if (digits.size() > shown) {
      const bool exact = rest.zero() && digits.find_first_not_of('0', shown) == std::string::npos;
      return std::format("{}{}{}e{}", sign, mantissa(digits.substr(0, shown), exact), exact ? "" : kMore, digits.size() - 1);
    }
    std::string fraction;
    while (digits.size() + fraction.size() < shown && !rest.zero()) fraction += next();
    const bool exact = rest.zero();
    return std::format("{}{}{}{}{}", sign, digits, fraction.empty() ? "" : ".", fraction, exact ? "" : kMore);
  }
  std::size_t zeros = 0;
  char first = next();
  for (; first == '0'; first = next()) ++zeros;
  std::string significant(1, first);
  while (significant.size() < shown && !rest.zero()) significant += next();
  const bool exact = rest.zero();
  if (zeros > kMaxLeadingZeros)
    return std::format("{}{}{}e-{}", sign, mantissa(significant, exact), exact ? "" : kMore, zeros + 1);
  return std::format("{}0.{}{}{}", sign, std::string(zeros, '0'), significant, exact ? "" : kMore);
}

// An irrational's digits as shown, the same way; always cut.
[[nodiscard]] inline std::string written(const decimal_digits &d) {
  const std::string_view sign = d.negative ? "-" : "";
  const auto count = static_cast<int>(d.digits.size());
  if (d.exponent >= count) return std::format("{}{}{}e{}", sign, mantissa(d.digits, false), kMore, d.exponent);
  if (d.exponent >= 0) {
    const auto whole = static_cast<std::size_t>(d.exponent + 1);
    return std::format("{}{}.{}{}", sign, std::string_view(d.digits).substr(0, whole), std::string_view(d.digits).substr(whole), kMore);
  }
  const auto zeros = static_cast<std::size_t>(-d.exponent - 1);
  if (zeros > kMaxLeadingZeros) return std::format("{}{}{}e{}", sign, mantissa(d.digits, false), kMore, d.exponent);
  return std::format("{}0.{}{}{}", sign, std::string(zeros, '0'), d.digits, kMore);
}

// A number as shown: a rational exactly; else its digits; one that cannot
// be told from zero, said so.
[[nodiscard]] inline std::string written(const number &x, std::size_t shown = kShownDigits) {
  if (const auto r = exact(x)) return written(*r, shown);
  if (const auto d = digits(x, shown)) return written(*d);
  return "≈0";
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

// How angles are read and written.
namespace angle {
struct radians {
  friend bool operator==(radians, radians) = default;
};
struct degrees {
  friend bool operator==(degrees, degrees) = default;
};
}  // namespace angle
using angle_t = std::variant<angle::radians, angle::degrees>;

[[nodiscard]] inline number to_radians(angle::radians, const number &x) { return x; }
[[nodiscard]] inline number to_radians(angle::degrees, const number &x) { return radians_of_degrees(x); }
[[nodiscard]] inline number from_radians(angle::radians, const number &x) { return x; }
[[nodiscard]] inline number from_radians(angle::degrees, const number &x) { return degrees_of_radians(x); }
[[nodiscard]] constexpr std::string_view name(angle::radians) { return "RAD"; }
[[nodiscard]] constexpr std::string_view name(angle::degrees) { return "DEG"; }

struct calc {
  // The number shown: as typed, or the last result as written().
  std::string entry = "0";
  // Its value, exactly (a result shown cut is kept whole here).
  number value;
  // The left side and the operation waiting for the right side.
  std::optional<number> left;
  std::optional<operation> pending;
  // What `=` did last, and its right side: `=` again does it again.
  std::optional<operation> repeat_op;
  number repeat_right;
  // The next digit starts a new entry (a result, or nothing typed yet).
  bool fresh = true;
  // A right side was given since the operation was chosen.
  bool given = false;
  // What is being worked out, as shown over the entry: "12 +", "12 + 3 =".
  std::string expression;
  // How angles are read and written.
  angle_t angles = angle::radians{};
  // How many digits a result is shown with: more asked for, more shown.
  std::size_t shown = kShownDigits;
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
// The constants, put in as a number typed is.
namespace constant {
struct pi {};
struct e {};
}  // namespace constant
using constant_t = std::variant<constant::pi, constant::e>;
struct put {
  constant_t which;
};
// The functions of the number shown, worked out at once.
namespace fn {
struct square_root {};
struct square {};
struct reciprocal {};
struct sine {};
struct cosine {};
struct tangent {};
struct arcsine {};
struct arccosine {};
struct arctangent {};
struct natural_log {};
struct common_log {};
struct exponential {};
}  // namespace fn
using function_t = std::variant<fn::square_root, fn::square, fn::reciprocal, fn::sine, fn::cosine, fn::tangent, fn::arcsine,
                                fn::arccosine, fn::arctangent, fn::natural_log, fn::common_log, fn::exponential>;
struct of {
  function_t which;
};
// Radians and degrees, in turn.
struct switch_angles {};
// More digits of the number shown.
struct more_digits {};

[[nodiscard]] constexpr std::string_view name(const constant::pi &) { return "π"; }
[[nodiscard]] constexpr std::string_view name(const constant::e &) { return "e"; }
[[nodiscard]] inline number value_of(const constant::pi &) { return number(rational(1), factor::pi{}); }
[[nodiscard]] inline number value_of(const constant::e &) { return number(rational(1), factor::exp_of{rational(1)}); }

[[nodiscard]] constexpr std::string_view name(fn::square_root) { return "√"; }
[[nodiscard]] constexpr std::string_view name(fn::square) { return "sqr"; }
[[nodiscard]] constexpr std::string_view name(fn::reciprocal) { return "1/"; }
[[nodiscard]] constexpr std::string_view name(fn::sine) { return "sin"; }
[[nodiscard]] constexpr std::string_view name(fn::cosine) { return "cos"; }
[[nodiscard]] constexpr std::string_view name(fn::tangent) { return "tan"; }
[[nodiscard]] constexpr std::string_view name(fn::arcsine) { return "asin"; }
[[nodiscard]] constexpr std::string_view name(fn::arccosine) { return "acos"; }
[[nodiscard]] constexpr std::string_view name(fn::arctangent) { return "atan"; }
[[nodiscard]] constexpr std::string_view name(fn::natural_log) { return "ln"; }
[[nodiscard]] constexpr std::string_view name(fn::common_log) { return "log"; }
[[nodiscard]] constexpr std::string_view name(fn::exponential) { return "exp"; }

// Each function, of a number, angles read and written as the calculator
// says.
[[nodiscard]] inline number apply(fn::square_root, const number &x, const angle_t &) { return sqrt(x); }
[[nodiscard]] inline number apply(fn::square, const number &x, const angle_t &) { return checked(x * x); }
[[nodiscard]] inline number apply(fn::reciprocal, const number &x, const angle_t &) {
  if (zero(x)) throw division_by_zero{};
  return reciprocal(x);
}
[[nodiscard]] inline number apply(fn::sine, const number &x, const angle_t &a) {
  return sin(std::visit([&](auto one) { return to_radians(one, x); }, a));
}
[[nodiscard]] inline number apply(fn::cosine, const number &x, const angle_t &a) {
  return cos(std::visit([&](auto one) { return to_radians(one, x); }, a));
}
[[nodiscard]] inline number apply(fn::tangent, const number &x, const angle_t &a) {
  return tan(std::visit([&](auto one) { return to_radians(one, x); }, a));
}
[[nodiscard]] inline number apply(fn::arcsine, const number &x, const angle_t &a) {
  return std::visit([&](auto one) { return from_radians(one, asin(x)); }, a);
}
[[nodiscard]] inline number apply(fn::arccosine, const number &x, const angle_t &a) {
  return std::visit([&](auto one) { return from_radians(one, acos(x)); }, a);
}
[[nodiscard]] inline number apply(fn::arctangent, const number &x, const angle_t &a) {
  return std::visit([&](auto one) { return from_radians(one, atan(x)); }, a);
}
[[nodiscard]] inline number apply(fn::natural_log, const number &x, const angle_t &) { return ln(x); }
[[nodiscard]] inline number apply(fn::common_log, const number &x, const angle_t &) { return log10(x); }
[[nodiscard]] inline number apply(fn::exponential, const number &x, const angle_t &) { return checked(exp(x)); }

// What went wrong, shown in place of the entry.
[[nodiscard]] inline calc failing(failure what, const calc &was) {
  calc out;
  out.failed = std::move(what);
  out.angles = was.angles;
  return out;
}
// A key that works something out: what its arithmetic throws caught here,
// and kept as the calculator's state.
template <class Work> [[nodiscard]] calc guarded(const calc &was, Work work) {
  try {
    return work();
  } catch (const division_by_zero &caught) {
    return failing(caught, was);
  } catch (const division_by_zero_t &) {
    return failing(division_by_zero{}, was);
  } catch (const out_of_range &caught) {
    return failing(caught, was);
  } catch (const not_defined &caught) {
    return failing(caught, was);
  } catch (const not_positive &) {
    return failing(not_defined{}, was);
  } catch (const too_close_to_zero &) {
    return failing(not_defined{}, was);
  }
}
// A result shown, and kept exactly.
inline void show(calc &c, number result) {
  c.shown = kShownDigits;
  c.entry = written(result, c.shown);
  c.value = std::move(result);
}
// The entry edited by typing: its value read again.
inline void typed(calc &c, std::string entry) {
  c.value = number(rational::from_decimal(entry));
  c.entry = std::move(entry);
}

[[nodiscard]] inline calc press(calc c, digit d) {
  if (c.failed) c = failing(*c.failed, c), c.failed.reset();
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
  if (c.failed) c = failing(*c.failed, c), c.failed.reset();
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

[[nodiscard]] inline calc press(calc c, put constant) {
  if (c.failed) c = failing(*c.failed, c), c.failed.reset();
  if (c.fresh && !c.pending) c.expression.clear();
  std::visit([&](const auto &one) { show(c, value_of(one)); }, constant.which);
  c.fresh = true;
  c.given = true;
  return c;
}

// An operation chosen: what was waiting worked out first (it may throw).
[[nodiscard]] inline calc chosen_on(calc c, choose chosen) {
  if (c.pending && c.given) {
    number result = apply(*c.pending, *c.left, c.value);
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
  const calc was = c;
  return guarded(was, [&] { return chosen_on(std::move(c), chosen); });
}

// `=`: the operation waiting, or the last one again (it may throw).
[[nodiscard]] inline calc equals_on(calc c) {
  number left;
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
  number result = apply(*c.repeat_op, left, c.repeat_right);
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
  const calc was = c;
  return guarded(was, [&] { return equals_on(std::move(c)); });
}

// A function of the number shown, worked out at once (it may throw).
[[nodiscard]] inline calc press(calc c, of f) {
  if (c.failed) return c;
  const calc was = c;
  return guarded(was, [&] {
    const std::string before = grouped(c.entry);
    number result = std::visit([&](const auto &one) { return apply(one, c.value, c.angles); }, f.which);
    if (!c.pending) c.expression = std::visit([&](const auto &one) { return std::format("{}({})", name(one), before); }, f.which);
    show(c, std::move(result));
    c.fresh = true;
    if (c.pending) c.given = true;
    return std::move(c);
  });
}

[[nodiscard]] inline calc press(calc c, clear) {
  calc out;
  out.angles = c.angles;
  return out;
}

[[nodiscard]] inline calc press(calc c, clear_entry) {
  if (c.failed) return press(std::move(c), clear{});
  typed(c, "0");
  c.fresh = true;
  c.given = c.pending.has_value();
  return c;
}

[[nodiscard]] inline calc press(calc c, backspace) {
  if (c.failed) return press(std::move(c), clear{});
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
  const calc was = c;
  return guarded(was, [&] {
    number made = c.pending ? std::visit([&](auto one) { return percent_of(one, *c.left, c.value); }, *c.pending) : hundredth(c.value);
    show(c, checked(std::move(made)));
    c.fresh = true;
    if (c.pending) c.given = true;
    return std::move(c);
  });
}

[[nodiscard]] inline angle_t switched(angle::radians) { return angle::degrees{}; }
[[nodiscard]] inline angle_t switched(angle::degrees) { return angle::radians{}; }
[[nodiscard]] inline calc press(calc c, switch_angles) {
  c.angles = std::visit([](auto one) { return switched(one); }, c.angles);
  return c;
}

// More digits: a result shown with twenty more (each still right); a
// number being typed is as typed.
[[nodiscard]] inline calc press(calc c, more_digits) {
  if (c.failed || !c.fresh) return c;
  c.shown += kShownDigits;
  c.entry = written(c.value, c.shown);
  return c;
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
// How angles are read now.
[[nodiscard]] inline std::string_view angles(const calc &c) {
  return std::visit([](auto one) { return name(one); }, c.angles);
}

}  // namespace calculator
