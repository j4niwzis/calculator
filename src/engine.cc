// SPDX-License-Identifier: AGPL-3.0-only
// The calculator itself, as Android's is: a formula typed whole -- numbers,
// operations, parentheses, functions, constants -- evaluated with
// precedence, its result shown under it as it is typed, and = making the
// result the number shown. Plain C++, no UI: the page (calculator.page)
// shows a calc and sends it keys; the tests press the keys themselves.
//
// Every number is kept as Boehm's calculator keeps it (calculator.symbolic):
// exact where it can be, each digit shown right where it cannot.
export module calculator.engine;

import std;
export import calculator.symbolic;

export namespace calculator {

// Callables, one per alternative: what std::visit is given.
template <class... F> struct overloaded : F... {
  using F::operator()...;
};
template <class... F> overloaded(F...) -> overloaded<F...>;

// ---- what can go wrong ------------------------------------------------------

// Thrown while a formula is evaluated; each says what it is (what()). = 
// catches them and shows the one caught under the formula.
struct division_by_zero : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Can't divide by 0"; }
  friend bool operator==(const division_by_zero &, const division_by_zero &) { return true; }
};
// A result too large to keep: over kMaxBits written down.
struct out_of_range : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Value too large"; }
  friend bool operator==(const out_of_range &, const out_of_range &) { return true; }
};
// A formula that is not one: an operation with nothing after it, a
// parenthesis closed that was never opened.
struct bad_expression : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Bad expression"; }
  friend bool operator==(const bad_expression &, const bad_expression &) { return true; }
};
using failure = std::variant<division_by_zero, out_of_range, not_defined, bad_expression>;

[[nodiscard]] inline std::string_view message(const failure &what) {
  return std::visit([](const auto &one) { return std::string_view(one.what()); }, what);
}

// The most bits a result keeps: some 39,000 decimal digits.
inline constexpr std::size_t kMaxBits = std::size_t{1} << 17;
[[nodiscard]] inline number checked(number v) {
  if (v.coefficient.bits() > kMaxBits) throw out_of_range{};
  return v;
}

// ---- the parts of a formula -------------------------------------------------

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

namespace constant {
struct pi {
  friend bool operator==(pi, pi) = default;
};
struct e {
  friend bool operator==(e, e) = default;
};
}  // namespace constant
using constant_t = std::variant<constant::pi, constant::e>;

// The functions, each written before what it is of: sin(, √, e^(...
namespace fn {
struct sine {
  friend bool operator==(sine, sine) = default;
};
struct cosine {
  friend bool operator==(cosine, cosine) = default;
};
struct tangent {
  friend bool operator==(tangent, tangent) = default;
};
struct arcsine {
  friend bool operator==(arcsine, arcsine) = default;
};
struct arccosine {
  friend bool operator==(arccosine, arccosine) = default;
};
struct arctangent {
  friend bool operator==(arctangent, arctangent) = default;
};
struct natural_log {
  friend bool operator==(natural_log, natural_log) = default;
};
struct common_log {
  friend bool operator==(common_log, common_log) = default;
};
struct exponential {
  friend bool operator==(exponential, exponential) = default;
};
struct ten_to {
  friend bool operator==(ten_to, ten_to) = default;
};
struct square_root {
  friend bool operator==(square_root, square_root) = default;
};
}  // namespace fn
using function_t = std::variant<fn::sine, fn::cosine, fn::tangent, fn::arcsine, fn::arccosine, fn::arctangent, fn::natural_log,
                                fn::common_log, fn::exponential, fn::ten_to, fn::square_root>;

// What is written after what it is of: n!, n%, n².
namespace post {
struct factorial {
  friend bool operator==(factorial, factorial) = default;
};
struct percent {
  friend bool operator==(percent, percent) = default;
};
struct squared {
  friend bool operator==(squared, squared) = default;
};
}  // namespace post
using postfix_t = std::variant<post::factorial, post::percent, post::squared>;

namespace token {
// A number being typed: its digits and point, as typed.
struct digits {
  std::string text;
  friend bool operator==(const digits &, const digits &) = default;
};
// A result carried on: the value = gave, and how it was shown.
struct value {
  number of;
  std::string shown;
  friend bool operator==(const value &, const value &) = default;
};
struct operation {
  calculator::operation which;
  friend bool operator==(const operation &, const operation &) = default;
};
struct open {
  friend bool operator==(open, open) = default;
};
struct close {
  friend bool operator==(close, close) = default;
};
struct function {
  function_t which;  // its parenthesis is the open after it
  friend bool operator==(const function &, const function &) = default;
};
struct constant {
  constant_t which;
  friend bool operator==(const constant &, const constant &) = default;
};
struct postfix {
  postfix_t which;
  friend bool operator==(const postfix &, const postfix &) = default;
};
}  // namespace token
using token_t = std::variant<token::digits, token::value, token::operation, token::open, token::close, token::function,
                             token::constant, token::postfix>;

// ---- what each part is written as ------------------------------------------

[[nodiscard]] constexpr std::string_view symbol(op::add) { return "+"; }
[[nodiscard]] constexpr std::string_view symbol(op::subtract) { return "−"; }
[[nodiscard]] constexpr std::string_view symbol(op::multiply) { return "×"; }
[[nodiscard]] constexpr std::string_view symbol(op::divide) { return "÷"; }
[[nodiscard]] constexpr std::string_view symbol(op::power) { return "^"; }
[[nodiscard]] constexpr std::string_view symbol(constant::pi) { return "π"; }
[[nodiscard]] constexpr std::string_view symbol(constant::e) { return "e"; }
[[nodiscard]] constexpr std::string_view symbol(fn::sine) { return "sin"; }
[[nodiscard]] constexpr std::string_view symbol(fn::cosine) { return "cos"; }
[[nodiscard]] constexpr std::string_view symbol(fn::tangent) { return "tan"; }
[[nodiscard]] constexpr std::string_view symbol(fn::arcsine) { return "sin⁻¹"; }
[[nodiscard]] constexpr std::string_view symbol(fn::arccosine) { return "cos⁻¹"; }
[[nodiscard]] constexpr std::string_view symbol(fn::arctangent) { return "tan⁻¹"; }
[[nodiscard]] constexpr std::string_view symbol(fn::natural_log) { return "ln"; }
[[nodiscard]] constexpr std::string_view symbol(fn::common_log) { return "log"; }
[[nodiscard]] constexpr std::string_view symbol(fn::exponential) { return "e^"; }
[[nodiscard]] constexpr std::string_view symbol(fn::ten_to) { return "10^"; }
[[nodiscard]] constexpr std::string_view symbol(fn::square_root) { return "√"; }
[[nodiscard]] constexpr std::string_view symbol(post::factorial) { return "!"; }
[[nodiscard]] constexpr std::string_view symbol(post::percent) { return "%"; }
[[nodiscard]] constexpr std::string_view symbol(post::squared) { return "²"; }

// ---- numbers as written -----------------------------------------------------

// The most digits a number typed takes; the significant digits a result is
// shown with under the formula, and once = makes it the number shown (the
// line scrolls: Boehm's display, every digit right however far it goes).
inline constexpr std::ptrdiff_t kMaxDigits = 20;
inline constexpr std::size_t kPreviewDigits = 15;
inline constexpr std::size_t kResultDigits = 60;
inline constexpr std::size_t kMaxLeadingZeros = 9;
inline constexpr std::string_view kMore = "…";

[[nodiscard]] inline std::ptrdiff_t digits_in(std::string_view text) {
  return std::ranges::count_if(text, [](char c) { return c >= '0' && c <= '9'; });
}

[[nodiscard]] inline std::string mantissa(std::string digits, bool exact) {
  if (exact) {
    const std::size_t last = digits.find_last_not_of('0');
    digits.resize(last == std::string::npos ? 1 : last + 1);
  }
  return digits.size() == 1 ? digits : std::format("{}.{}", digits.front(), std::string_view(digits).substr(1));
}

// A rational as shown: exact where it ends within `shown` significant
// digits; else cut, with an ellipsis; an exponent where it is too large or
// small for them.
[[nodiscard]] inline std::string written(const rational &q, std::size_t shown) {
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
  if (zeros > kMaxLeadingZeros) return std::format("{}{}{}e-{}", sign, mantissa(significant, exact), exact ? "" : kMore, zeros + 1);
  return std::format("{}0.{}{}{}", sign, std::string(zeros, '0'), significant, exact ? "" : kMore);
}
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
[[nodiscard]] inline std::string written(const number &x, std::size_t shown) {
  if (const auto r = exact(x)) return written(*r, shown);
  if (const auto d = digits(x, shown)) return written(*d);
  return "≈0";
}

// Digits grouped by thousands from the right: 1234567 -> 1,234,567.
[[nodiscard]] inline std::string grouped_digits(std::string_view digits) {
  std::string out = digits | std::views::reverse | std::views::enumerate | std::views::transform([](auto at) {
                      const auto [i, c] = at;
                      return i > 0 && i % 3 == 0 ? std::string{',', c} : std::string(1, c);
                    }) |
                    std::views::join | std::ranges::to<std::string>();
  std::ranges::reverse(out);
  return out;
}
// A number as shown: its whole part grouped by thousands, the rest as it is.
[[nodiscard]] inline std::string grouped(std::string_view text) {
  const bool negative = text.starts_with('-');
  const std::string_view body = text.substr(negative ? 1 : 0);
  const std::size_t whole = std::min(body.find_first_not_of("0123456789"), body.size());
  return std::format("{}{}{}", negative ? "−" : "", grouped_digits(body.substr(0, whole)), body.substr(whole));
}

// ---- angles --------------------------------------------------------------------

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
[[nodiscard]] inline angle_t switched(angle::radians) { return angle::degrees{}; }
[[nodiscard]] inline angle_t switched(angle::degrees) { return angle::radians{}; }

// ---- evaluating ---------------------------------------------------------------

[[nodiscard]] inline number value_of(constant::pi) { return number(rational(1), factor::pi{}); }
[[nodiscard]] inline number value_of(constant::e) { return number(rational(1), factor::exp_of{rational(1)}); }

[[nodiscard]] inline number apply(op::add, const number &a, const number &b) { return checked(a + b); }
[[nodiscard]] inline number apply(op::subtract, const number &a, const number &b) { return checked(a - b); }
[[nodiscard]] inline number apply(op::multiply, const number &a, const number &b) { return checked(a * b); }
[[nodiscard]] inline number apply(op::divide, const number &a, const number &b) {
  if (zero(b)) throw division_by_zero{};
  return checked(a / b);
}
[[nodiscard]] inline number apply(op::power, const number &a, const number &b) { return checked(pow(a, b)); }

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
[[nodiscard]] inline number apply(fn::ten_to, const number &x, const angle_t &) { return checked(pow(number(rational(10)), x)); }
[[nodiscard]] inline number apply(fn::square_root, const number &x, const angle_t &) { return sqrt(x); }

// n! for a whole n from 0 to 1000; nothing else has one here.
[[nodiscard]] inline number apply(post::factorial, const number &x) {
  const auto r = exact(x);
  const auto n = r.and_then([](const rational &q) { return small_integer(q); });
  if (!n || *n < 0 || *n > 1000) throw not_defined{};
  rational out(1);
  for (std::int64_t k = 2; k <= *n; ++k) out = out * rational(k);
  return checked(number(out));
}
[[nodiscard]] inline number apply(post::percent, const number &x) { return x * number(rational(1) / rational(100)); }
[[nodiscard]] inline number apply(post::squared, const number &x) { return checked(x * x); }

// What the last token is, for what a key does after it.
[[nodiscard]] inline bool ends_a_number(const token_t &t) {
  return std::visit(overloaded{[](const token::digits &) { return true; }, [](const token::value &) { return true; },
                               [](const token::close &) { return true; }, [](const token::constant &) { return true; },
                               [](const token::postfix &) { return true; }, [](const auto &) { return false; }},
                    t);
}
// How far a token takes the parentheses open: +1, -1, 0.
[[nodiscard]] inline std::ptrdiff_t depth_of(const token_t &t) {
  return std::visit(overloaded{[](const token::open &) { return std::ptrdiff_t{1}; }, [](const token::close &) { return std::ptrdiff_t{-1}; },
                               [](const auto &) { return std::ptrdiff_t{0}; }},
                    t);
}
[[nodiscard]] inline std::ptrdiff_t open_count(const std::vector<token_t> &formula) {
  return std::ranges::fold_left(formula | std::views::transform(depth_of), std::ptrdiff_t{0}, std::plus<>{});
}
[[nodiscard]] inline bool is_function(const token_t &t) {
  return std::visit(overloaded{[](const token::function &) { return true; }, [](const auto &) { return false; }}, t);
}
// + and - (after which a minus is not a sign, but replaces them).
[[nodiscard]] inline bool additive(const operation &o) {
  return std::visit(overloaded{[](op::add) { return true; }, [](op::subtract) { return true; }, [](auto) { return false; }}, o);
}
[[nodiscard]] inline bool minus(const operation &o) {
  return std::visit(overloaded{[](op::subtract) { return true; }, [](auto) { return false; }}, o);
}

// A Pratt parser over the formula's tokens: + and - lowest, then x and /
// (and a product written by putting two things side by side: 2pi, 3(4+1)),
// then a sign, then ^ (to the right), then what is written after (!, %, ²).
// A parenthesis left open at the end is closed there, as Android does.
class evaluation {
 public:
  evaluation(const std::vector<token_t> &formula, const angle_t &angles) : formula_(formula), angles_(angles) {}

  [[nodiscard]] number run() {
    if (formula_.empty()) throw bad_expression{};
    number result = expression(0);
    if (at_ < formula_.size()) throw bad_expression{};
    return result;
  }

 private:
  const std::vector<token_t> &formula_;
  const angle_t &angles_;
  std::size_t at_ = 0;

  static constexpr int kSum = 10, kProduct = 20, kSign = 30, kPower = 40, kPostfix = 50;

  [[nodiscard]] const token_t *peek() const { return at_ < formula_.size() ? &formula_[at_] : nullptr; }

  // After a number, what the next token does with it, by its kind: an
  // operation binds by its precedence; a postfix applies at once; the start
  // of a factor is a product unwritten (2pi, 3(4+1)); a close, or the end of
  // what this level binds, stops. True: it went on.
  struct after_number {
    evaluation &self;
    number &left;
    int min;
    bool operator()(const token::operation &o) const {
      const int binds = std::visit(overloaded{[](op::add) { return kSum; }, [](op::subtract) { return kSum; },
                                              [](op::multiply) { return kProduct; }, [](op::divide) { return kProduct; },
                                              [](op::power) { return kPower; }},
                                   o.which);
      if (binds <= min) return false;
      ++self.at_;
      // ^ to the right: 2^3^2 is 2^9; the others to the left.
      const number rhs = self.expression(binds == kPower ? kPower - 1 : binds);
      left = std::visit([&](auto one) { return apply(one, left, rhs); }, o.which);
      return true;
    }
    bool operator()(const token::postfix &p) const {
      if (kPostfix <= min) return false;
      ++self.at_;
      left = std::visit([&](auto one) { return apply(one, left); }, p.which);
      return true;
    }
    bool operator()(const token::close &) const { return false; }
    template <class Factor> bool operator()(const Factor &) const {
      if (kProduct <= min) return false;
      left = apply(op::multiply{}, left, self.expression(kProduct));
      return true;
    }
  };

  [[nodiscard]] number expression(int min) {
    number left = prefix();
    while (const token_t *next = peek())
      if (!std::visit(after_number{*this, left, min}, *next)) break;
    return left;
  }

  [[nodiscard]] number prefix() {
    const token_t *next = peek();
    if (next == nullptr) throw bad_expression{};
    ++at_;
    return std::visit(overloaded{
                          [&](const token::digits &d) { return number(rational::from_decimal(d.text)); },
                          [&](const token::value &v) { return v.of; },
                          [&](const token::constant &c) { return std::visit([](auto one) { return value_of(one); }, c.which); },
                          [&](const token::open &) { return group(); },
                          [&](const token::function &f) {
                            // Its own parenthesis, or a factor: sqrt 2.
                            const token_t *paren = peek();
                            const number x = paren && depth_of(*paren) > 0 ? (++at_, group()) : expression(kSign);
                            return std::visit([&](auto one) { return apply(one, x, angles_); }, f.which);
                          },
                          [&](const token::operation &o) {
                            // A sign: - negates what follows, + leaves it.
                            const number x = expression(kSign);
                            return std::visit(overloaded{[&](op::subtract) { return -x; }, [&](op::add) { return x; },
                                                         [](auto) -> number { throw bad_expression{}; }},
                                              o.which);
                          },
                          [](const token::close &) -> number { throw bad_expression{}; },
                          [](const token::postfix &) -> number { throw bad_expression{}; }},
                      *next);
  }

  // After an open parenthesis: what is in it, and its close (or the end).
  [[nodiscard]] number group() {
    const number inside = expression(0);
    if (const token_t *next = peek()) {
      if (depth_of(*next) >= 0) throw bad_expression{};  // only a close ends a group
      ++at_;
    }
    return inside;
  }
};

[[nodiscard]] inline number evaluate(const std::vector<token_t> &formula, const angle_t &angles) {
  return evaluation(formula, angles).run();
}

// ---- the calculator ------------------------------------------------------------

struct calc {
  // What is typed.
  std::vector<token_t> formula;
  // After =: the result, shown as the number (formula then holds it alone,
  // as a value carried on).
  bool showing_result = false;
  // After = on a formula that has no value: why.
  std::optional<failure> failed;
  angle_t angles = angle::radians{};
  // The inverse functions on the keys (sin⁻¹ for sin, eˣ for ln...).
  bool inverse = false;
  // The scientific keys all shown.
  bool expanded = false;
  friend bool operator==(const calc &, const calc &) = default;
};

// ---- the keys -------------------------------------------------------------------

struct digit {
  int value = 0;
};
struct point {};
struct choose {
  operation which;
};
struct put {
  constant_t which;
};
struct of {
  function_t which;
};
struct after {
  postfix_t which;
};
// The one parenthesis key: an open, or a close where one is open and a
// number ends what is typed.
struct parentheses {};
struct equals {};
struct clear {};
struct backspace {};
struct switch_angles {};
struct switch_inverse {};
struct expand {};

// A key that starts something new after a result: the result is let go
// (a number typed starts a new formula).
inline void start_over_if_result(calc &c) {
  if (c.showing_result) {
    c.formula.clear();
    c.showing_result = false;
  }
  c.failed.reset();
}
// A key that goes on from a result: the result stays, carried on.
inline void go_on(calc &c) {
  c.showing_result = false;
  c.failed.reset();
}

// The number being typed, where the formula ends in one: edited by `f`,
// which says whether it took the key.
template <class F> [[nodiscard]] bool on_last_digits(calc &c, F f) {
  if (c.formula.empty()) return false;
  return std::visit(overloaded{[&](token::digits &d) { return f(d); }, [](auto &) { return false; }}, c.formula.back());
}

[[nodiscard]] inline calc press(calc c, digit d) {
  start_over_if_result(c);
  const char typed = static_cast<char>('0' + std::clamp(d.value, 0, 9));
  const bool went_on = on_last_digits(c, [&](token::digits &last) {
    if (last.text == "0") last.text = std::string(1, typed);
    else if (digits_in(last.text) < kMaxDigits) last.text += typed;
    return true;
  });
  if (!went_on) c.formula.push_back(token::digits{std::string(1, typed)});
  return c;
}

[[nodiscard]] inline calc press(calc c, point) {
  start_over_if_result(c);
  const bool went_on = on_last_digits(c, [](token::digits &last) {
    if (!last.text.contains('.')) last.text += '.';
    return true;
  });
  if (!went_on) c.formula.push_back(token::digits{"0."});
  return c;
}

[[nodiscard]] inline calc press(calc c, choose chosen) {
  go_on(c);
  // An operation after an operation replaces it -- but a minus after x, /
  // or ^ is a sign (2 x -3).
  const bool replaced = !c.formula.empty() &&
                        std::visit(overloaded{[&](token::operation &last) {
                                                if (minus(chosen.which) && !additive(last.which)) return false;
                                                last.which = chosen.which;
                                                return true;
                                              },
                                              [](auto &) { return false; }},
                                   c.formula.back());
  if (replaced) return c;
  // Nothing before it: only a minus, a sign.
  if (c.formula.empty() && !minus(chosen.which)) return c;
  c.formula.push_back(token::operation{chosen.which});
  return c;
}

[[nodiscard]] inline calc press(calc c, put constant) {
  start_over_if_result(c);
  c.formula.push_back(token::constant{constant.which});
  return c;
}

[[nodiscard]] inline calc press(calc c, of f) {
  start_over_if_result(c);
  c.formula.push_back(token::function{f.which});
  c.formula.push_back(token::open{});
  return c;
}

[[nodiscard]] inline calc press(calc c, after p) {
  go_on(c);
  if (c.formula.empty() || !ends_a_number(c.formula.back())) return c;
  c.formula.push_back(token::postfix{p.which});
  return c;
}

[[nodiscard]] inline calc press(calc c, parentheses) {
  if (c.showing_result) go_on(c);
  c.failed.reset();
  if (open_count(c.formula) > 0 && !c.formula.empty() && ends_a_number(c.formula.back()))
    c.formula.push_back(token::close{});
  else
    c.formula.push_back(token::open{});
  return c;
}

[[nodiscard]] inline calc press(calc c, backspace) {
  if (c.showing_result) {
    c.formula.clear();
    c.showing_result = false;
    return c;
  }
  c.failed.reset();
  if (c.formula.empty()) return c;
  const bool shortened = on_last_digits(c, [](token::digits &last) {
    if (last.text.size() < 2) return false;
    last.text.pop_back();
    return true;
  });
  if (shortened) return c;
  c.formula.pop_back();
  // A function goes with its parenthesis.
  if (!c.formula.empty() && is_function(c.formula.back())) c.formula.pop_back();
  return c;
}

[[nodiscard]] inline calc press(calc c, clear) {
  c.formula.clear();
  c.showing_result = false;
  c.failed.reset();
  return c;
}

[[nodiscard]] inline calc press(calc c, equals) {
  if (c.formula.empty() || c.showing_result) return c;
  try {
    const number result = evaluate(c.formula, c.angles);
    c.formula = {token::value{result, written(result, kResultDigits)}};
    c.showing_result = true;
  } catch (const division_by_zero &caught) {
    c.failed = caught;
  } catch (const division_by_zero_t &) {
    c.failed = division_by_zero{};
  } catch (const out_of_range &caught) {
    c.failed = caught;
  } catch (const not_defined &caught) {
    c.failed = caught;
  } catch (const bad_expression &caught) {
    c.failed = caught;
  } catch (const not_positive &) {
    c.failed = not_defined{};
  } catch (const too_close_to_zero &) {
    c.failed = not_defined{};
  } catch (const too_large &) {
    c.failed = out_of_range{};
  }
  return c;
}

[[nodiscard]] inline calc press(calc c, switch_angles) {
  c.angles = std::visit([](auto one) { return switched(one); }, c.angles);
  return c;
}
[[nodiscard]] inline calc press(calc c, switch_inverse) {
  c.inverse = !c.inverse;
  return c;
}
[[nodiscard]] inline calc press(calc c, expand) {
  c.expanded = !c.expanded;
  return c;
}

// A key, as an edit of the calculator the page keeps (model::over).
template <class Key> struct pressed {
  Key key;
  [[nodiscard]] calc operator()(calc c) const { return press(std::move(c), key); }
};

// ---- what is shown ----------------------------------------------------------------

[[nodiscard]] inline std::string text_of(const token::digits &d) { return grouped(d.text); }
[[nodiscard]] inline std::string text_of(const token::value &v) { return grouped(v.shown); }
[[nodiscard]] inline std::string text_of(const token::operation &o) {
  return std::string(std::visit([](auto one) { return symbol(one); }, o.which));
}
[[nodiscard]] inline std::string text_of(const token::open &) { return "("; }
[[nodiscard]] inline std::string text_of(const token::close &) { return ")"; }
[[nodiscard]] inline std::string text_of(const token::function &f) {
  return std::string(std::visit([](auto one) { return symbol(one); }, f.which));
}
[[nodiscard]] inline std::string text_of(const token::constant &k) {
  return std::string(std::visit([](auto one) { return symbol(one); }, k.which));
}
[[nodiscard]] inline std::string text_of(const token::postfix &p) {
  return std::string(std::visit([](auto one) { return symbol(one); }, p.which));
}

// The formula as typed; after =, the result.
[[nodiscard]] inline std::string formula_text(const calc &c) {
  return c.formula | std::views::transform([](const token_t &t) { return std::visit([](const auto &one) { return text_of(one); }, t); }) |
         std::views::join | std::ranges::to<std::string>();
}
// The line under it: the formula's value as typed (where it has one and is
// more than a number), what went wrong after =, or nothing.
[[nodiscard]] inline std::string result_text(const calc &c) {
  if (c.failed) return std::string(message(*c.failed));
  if (c.showing_result || c.formula.size() < 2) return std::string();
  // A formula still being typed has no value yet: nothing shown, nothing
  // said wrong.
  try {
    return grouped(written(evaluate(c.formula, c.angles), kPreviewDigits));
  } catch (const bad_expression &) {
  } catch (const division_by_zero &) {
  } catch (const division_by_zero_t &) {
  } catch (const out_of_range &) {
  } catch (const not_defined &) {
  } catch (const not_positive &) {
  } catch (const too_close_to_zero &) {
  } catch (const too_large &) {
  }
  return std::string();
}
[[nodiscard]] inline std::string_view angles(const calc &c) {
  return std::visit([](auto one) { return name(one); }, c.angles);
}

}  // namespace calculator
