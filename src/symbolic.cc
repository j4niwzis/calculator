// SPDX-License-Identifier: AGPL-3.0-only
// A number as the calculator keeps it: a rational times a factor -- 1, pi,
// a square root, e to a power, a logarithm, a trigonometric value of a
// rational multiple of pi, an inverse trigonometric value -- or, where none
// of those fits, a constructive real (calculator.real). Boehm's design for
// Android's calculator ("Towards an API for the real numbers", PLDI 2020):
// the factor said symbolically keeps what can be exact exact -- pi + 3 pi
// is 4 pi, sqrt 8 is 2 sqrt 2, sin(pi/6) is 1/2, ln 4 - 2 ln 2 is 0 -- and
// only what no rule covers is left to the reals, whose digits are still
// each right.
export module calculator.symbolic;

import std;
export import calculator.number;
export import calculator.real;

export namespace calculator {

// What has no value: the square root or the logarithm of a negative number,
// tan(pi/2), asin of 2, 0 to a negative power.
struct not_defined : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Not defined"; }
  friend bool operator==(const not_defined &, const not_defined &) { return true; }
};

namespace factor {
struct one {
  friend bool operator==(one, one) = default;
};
struct pi {
  friend bool operator==(pi, pi) = default;
};
// sqrt(of): of an integer above 1 with no square factor found.
struct sqrt_of {
  rational of;
  friend bool operator==(const sqrt_of &, const sqrt_of &) = default;
};
// e^of, of not zero.
struct exp_of {
  rational of;
  friend bool operator==(const exp_of &, const exp_of &) = default;
};
// ln(of), of above zero and not 1.
struct ln_of {
  rational of;
  friend bool operator==(const ln_of &, const ln_of &) = default;
};
// log10(of), of above zero and not a power of ten.
struct log_of {
  rational of;
  friend bool operator==(const log_of &, const log_of &) = default;
};
// sin(pi of), of between 0 and 1, not a sixth or a quarter.
struct sin_pi_of {
  rational of;
  friend bool operator==(const sin_pi_of &, const sin_pi_of &) = default;
};
// tan(pi of), of between 0 and 1, not a sixth or a quarter or a half.
struct tan_pi_of {
  rational of;
  friend bool operator==(const tan_pi_of &, const tan_pi_of &) = default;
};
// asin(of), atan(of): of rational, not one of the values whose angle is a
// rational multiple of pi kept as one.
struct asin_of {
  rational of;
  friend bool operator==(const asin_of &, const asin_of &) = default;
};
struct atan_of {
  rational of;
  friend bool operator==(const atan_of &, const atan_of &) = default;
};
// A constructive real: nothing more known of it.
struct other {
  real value;
  friend bool operator==(const other &, const other &) = default;
};
}  // namespace factor
using factor_t = std::variant<factor::one, factor::pi, factor::sqrt_of, factor::exp_of, factor::ln_of, factor::log_of,
                              factor::sin_pi_of, factor::tan_pi_of, factor::asin_of, factor::atan_of, factor::other>;

struct number {
  rational coefficient;
  factor_t factor = factor::one{};
  number() = default;
  explicit number(rational value) : coefficient(std::move(value)) {}
  number(rational c, factor_t f) : coefficient(std::move(c)), factor(std::move(f)) {
    if (coefficient.zero()) factor = factor::one{};
  }
  friend bool operator==(const number &, const number &) = default;
};

// ---- what a number is -------------------------------------------------------

[[nodiscard]] inline bool zero(const number &x) { return x.coefficient.zero(); }
// Its value where it is rational: a factor of 1, or nothing at all.
struct rational_part {
  const rational &coefficient;
  std::optional<rational> operator()(const factor::one &) const { return coefficient; }
  template <class F> std::optional<rational> operator()(const F &) const {
    return coefficient.zero() ? std::optional<rational>(coefficient) : std::nullopt;
  }
};
[[nodiscard]] inline std::optional<rational> exact(const number &x) { return std::visit(rational_part{x.coefficient}, x.factor); }

[[nodiscard]] inline real real_of(const factor::one &) { return real(1); }
[[nodiscard]] inline real real_of(const factor::pi &) { return pi(); }
[[nodiscard]] inline real real_of(const factor::sqrt_of &f) { return sqrt(real(f.of)); }
[[nodiscard]] inline real real_of(const factor::exp_of &f) { return exp(real(f.of)); }
[[nodiscard]] inline real real_of(const factor::ln_of &f) { return ln(real(f.of)); }
[[nodiscard]] inline real real_of(const factor::log_of &f) {
  static const real ln10 = ln(real(10));
  return ln(real(f.of)) / ln10;
}
[[nodiscard]] inline real real_of(const factor::sin_pi_of &f) { return sin(pi() * real(f.of)); }
[[nodiscard]] inline real real_of(const factor::tan_pi_of &f) { return tan(pi() * real(f.of)); }
[[nodiscard]] inline real real_of(const factor::asin_of &f) { return asin(real(f.of)); }
[[nodiscard]] inline real real_of(const factor::atan_of &f) { return atan(real(f.of)); }
[[nodiscard]] inline real real_of(const factor::other &f) { return f.value; }
[[nodiscard]] inline real real_of(const number &x) {
  return std::visit([&](const auto &f) { return real(x.coefficient) * real_of(f); }, x.factor);
}

// The sign of each factor: those always positive; a logarithm by its
// argument; sin and tan of pi times a number by the half turn it is in; the
// inverses by their argument; a real asked.
[[nodiscard]] inline int sign_of(const factor::one &) { return 1; }
[[nodiscard]] inline int sign_of(const factor::pi &) { return 1; }
[[nodiscard]] inline int sign_of(const factor::sqrt_of &) { return 1; }
[[nodiscard]] inline int sign_of(const factor::exp_of &) { return 1; }
[[nodiscard]] inline int sign_of(const factor::ln_of &f) { return f.of > rational(1) ? 1 : -1; }
[[nodiscard]] inline int sign_of(const factor::log_of &f) { return f.of > rational(1) ? 1 : -1; }
[[nodiscard]] inline int sign_of(const factor::sin_pi_of &) { return 1; }
[[nodiscard]] inline int sign_of(const factor::tan_pi_of &f) { return f.of < rational(1) / rational(2) ? 1 : -1; }
[[nodiscard]] inline int sign_of(const factor::asin_of &f) { return f.of.negative() ? -1 : 1; }
[[nodiscard]] inline int sign_of(const factor::atan_of &f) { return f.of.negative() ? -1 : 1; }
[[nodiscard]] inline int sign_of(const factor::other &f) { return f.value.sign(); }
[[nodiscard]] inline int sign(const number &x) {
  if (zero(x)) return 0;
  const int s = std::visit([](const auto &f) { return sign_of(f); }, x.factor);
  return x.coefficient.negative() ? -s : s;
}

// ---- rationals, by rule ----------------------------------------------------

[[nodiscard]] inline rational power(rational base, std::int64_t k) {
  if (k < 0) {
    if (base.zero()) throw not_defined{};
    base = rational(1) / base;
    k = -k;
  }
  rational out(1);
  for (; k > 0; k >>= 1) {
    if (k & 1) out = out * base;
    base = base * base;
  }
  return out;
}
// A rational's integer value where it is one that fits.
[[nodiscard]] inline std::optional<std::int64_t> small_integer(const rational &q) {
  if (!q.whole() || q.numerator().bits() > 62) return std::nullopt;
  const std::uint64_t magnitude = (std::uint64_t{q.numerator().shifted_right(32).low()} << 32) | q.numerator().low();
  return q.negative() ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
}
// n = s^2 t: the square factors taken out of n where they are found (primes
// below 1000, and an n that is a square whole).
[[nodiscard]] inline std::pair<natural, natural> square_part(natural n) {
  natural s(1);
  for (std::uint32_t p = 2; p < 1000 && !n.zero(); ++p) {
    const natural pp(std::uint64_t{p} * p);
    for (;;) {
      auto [q, r] = divmod(n, pp);
      if (!r.zero()) break;
      n = std::move(q);
      s = s * natural(p);
    }
  }
  const natural root = n.square_root();
  if (root * root == n) return {s * root, natural(1)};
  return {s, n};
}

// ---- the arithmetic --------------------------------------------------------

[[nodiscard]] inline number operator-(const number &x) { return number(-x.coefficient, x.factor); }

// k ln a + m ln b, k and m small integers: ln(a^k b^m), one logarithm.
template <class Log>
[[nodiscard]] inline std::optional<number> logs_added(const rational &k, const Log &a, const rational &m, const Log &b) {
  const auto ki = small_integer(k);
  const auto mi = small_integer(m);
  if (!ki || !mi || std::abs(*ki) > 64 || std::abs(*mi) > 64) return std::nullopt;
  const rational of = power(a.of, *ki) * power(b.of, *mi);
  if (of == rational(1)) return number();
  return number(rational(1), Log{of});
}

// The pairs of factors whose sum has a rule: two logarithms of one base.
struct sum_rule {
  const number &a, &b;
  std::optional<number> operator()(const factor::ln_of &x, const factor::ln_of &y) const {
    return logs_added(a.coefficient, x, b.coefficient, y);
  }
  std::optional<number> operator()(const factor::log_of &x, const factor::log_of &y) const {
    return logs_added(a.coefficient, x, b.coefficient, y);
  }
  template <class X, class Y> std::optional<number> operator()(const X &, const Y &) const { return std::nullopt; }
};

[[nodiscard]] inline number operator+(const number &a, const number &b) {
  if (zero(a)) return b;
  if (zero(b)) return a;
  if (a.factor == b.factor) return number(a.coefficient + b.coefficient, a.factor);
  if (auto sum = std::visit(sum_rule{a, b}, a.factor, b.factor)) return *sum;
  return number(rational(1), factor::other{real_of(a) + real_of(b)});
}
[[nodiscard]] inline number operator-(const number &a, const number &b) { return a + -b; }

// sqrt of a positive rational, its square factors taken out:
// sqrt(p/q) = sqrt(p q) / q = s sqrt(t) / q.
[[nodiscard]] inline number sqrt_of_rational(const rational &r) {
  if (r.negative()) throw not_defined{};
  if (r.zero()) return number();
  const auto [s, t] = square_part(r.numerator() * r.denominator());
  const rational c(false, s, r.denominator());
  if (t == natural(1)) return number(c);
  return number(c, factor::sqrt_of{rational(false, t, natural(1))});
}

// The pairs of factors whose product has a rule: a rational keeps the other
// factor; two roots are one; two powers of e are one.
struct product_rule {
  const number &a, &b;
  rational c;
  number operator()(const factor::one &, const factor::one &) const { return number(c); }
  template <class Y> number operator()(const factor::one &, const Y &y) const { return number(c, y); }
  template <class X> number operator()(const X &x, const factor::one &) const { return number(c, x); }
  number operator()(const factor::sqrt_of &x, const factor::sqrt_of &y) const {
    const number root = sqrt_of_rational(x.of * y.of);
    return number(c * root.coefficient, root.factor);
  }
  number operator()(const factor::exp_of &x, const factor::exp_of &y) const {
    const rational of = x.of + y.of;
    return of.zero() ? number(c) : number(c, factor::exp_of{of});
  }
  template <class X, class Y> number operator()(const X &, const Y &) const {
    return number(rational(1), factor::other{real_of(a) * real_of(b)});
  }
};

[[nodiscard]] inline number operator*(const number &a, const number &b) {
  if (zero(a) || zero(b)) return number();
  return std::visit(product_rule{a, b, a.coefficient * b.coefficient}, a.factor, b.factor);
}

struct division_by_zero_t : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Cannot divide by zero"; }
};

[[nodiscard]] inline number reciprocal_of(const rational &c, const factor::one &) { return number(rational(1) / c); }
[[nodiscard]] inline number reciprocal_of(const rational &c, const factor::sqrt_of &f) {
  return number(rational(1) / (c * f.of), f);
}
[[nodiscard]] inline number reciprocal_of(const rational &c, const factor::exp_of &f) {
  return number(rational(1) / c, factor::exp_of{-f.of});
}
template <class F> [[nodiscard]] inline number reciprocal_of(const rational &c, const F &f) {
  return number(rational(1) / c, factor::other{real_of(f).inverse()});
}
// 1/x, x not zero (a zero throws division_by_zero_t; a real too close to
// zero to tell throws too_close_to_zero).
[[nodiscard]] inline number reciprocal(const number &x) {
  if (zero(x)) throw division_by_zero_t{};
  return std::visit([&](const auto &f) { return reciprocal_of(x.coefficient, f); }, x.factor);
}
[[nodiscard]] inline number operator/(const number &a, const number &b) { return a * reciprocal(b); }

// ---- the functions ---------------------------------------------------------

// sqrt(c e^r) = sqrt(c) e^(r/2), where sqrt(c) is rational; else a real.
struct sqrt_rule {
  const number &x;
  number operator()(const factor::exp_of &e) const;
  template <class F> number operator()(const F &) const { return number(rational(1), factor::other{sqrt(real_of(x))}); }
};
[[nodiscard]] inline number sqrt(const number &x) {
  if (const auto r = exact(x)) return sqrt_of_rational(*r);
  if (sign(x) < 0) throw not_defined{};
  return std::visit(sqrt_rule{x}, x.factor);
}
inline number sqrt_rule::operator()(const factor::exp_of &e) const {
  const number root = sqrt_of_rational(x.coefficient);
  if (const auto c = exact(root)) return number(*c, factor::exp_of{e.of / rational(2)});
  return number(rational(1), factor::other{sqrt(real_of(x))});
}

// e^(k ln r) = r^k, k a small integer; else a real.
struct exp_rule {
  const number &x;
  number operator()(const factor::ln_of &l) const {
    if (const auto k = small_integer(x.coefficient); k && std::abs(*k) <= 64) return number(power(l.of, *k));
    return number(rational(1), factor::other{exp(real_of(x))});
  }
  template <class F> number operator()(const F &) const { return number(rational(1), factor::other{exp(real_of(x))}); }
};
[[nodiscard]] inline number exp(const number &x) {
  if (const auto r = exact(x)) return r->zero() ? number(rational(1)) : number(rational(1), factor::exp_of{*r});
  return std::visit(exp_rule{x}, x.factor);
}

// ln(c e^r) = r + ln c; else a real.
struct ln_rule {
  const number &x;
  number operator()(const factor::exp_of &e) const;
  template <class F> number operator()(const F &) const { return number(rational(1), factor::other{ln(real_of(x))}); }
};
[[nodiscard]] inline number ln(const number &x) {
  if (sign(x) <= 0) throw not_defined{};
  if (const auto r = exact(x)) return *r == rational(1) ? number() : number(rational(1), factor::ln_of{*r});
  return std::visit(ln_rule{x}, x.factor);
}
inline number ln_rule::operator()(const factor::exp_of &e) const { return number(e.of) + ln(number(x.coefficient)); }

// log10: a power of ten its exponent.
[[nodiscard]] inline std::optional<std::int64_t> power_of_ten_exponent(const rational &r) {
  for (std::int64_t k = -400; k <= 400; ++k)
    if (r == power(rational(10), k)) return k;
  return std::nullopt;
}
[[nodiscard]] inline number log10(const number &x) {
  if (sign(x) <= 0) throw not_defined{};
  if (const auto r = exact(x)) {
    if (const auto k = power_of_ten_exponent(*r)) return number(rational(*k));
    return number(rational(1), factor::log_of{*r});
  }
  return number(rational(1), factor::other{ln(real_of(x)) / ln(real(10))});
}

// q taken into [0, period).
[[nodiscard]] inline rational reduced(const rational &q, const rational &period) {
  const rational turns = q / period;
  const integer whole = turns.signed_numerator() / integer(false, turns.denominator());
  rational r = q - rational(whole) * period;
  if (r.negative()) r = r + period;
  return r;
}
[[nodiscard]] inline rational sixth(std::int64_t k) { return rational(k) / rational(6); }
[[nodiscard]] inline rational quarter(std::int64_t k) { return rational(k) / rational(4); }
[[nodiscard]] inline rational half() { return rational(1) / rational(2); }

// sin(pi q): exact at the sixths and the quarters of a turn's half.
[[nodiscard]] inline number sin_pi(const rational &q) {
  const rational r = reduced(q, rational(2));
  if (r >= rational(1)) return -sin_pi(r - rational(1));
  if (r.zero()) return number();
  if (r == sixth(1) || r == sixth(5)) return number(half());
  if (r == half()) return number(rational(1));
  if (r == quarter(1) || r == quarter(3)) return number(half(), factor::sqrt_of{rational(2)});
  if (r == sixth(2) || r == sixth(4)) return number(half(), factor::sqrt_of{rational(3)});
  return number(rational(1), factor::sin_pi_of{r});
}
[[nodiscard]] inline number cos_pi(const rational &q) { return sin_pi(q + half()); }
[[nodiscard]] inline number tan_pi(const rational &q) {
  const rational r = reduced(q, rational(1));
  if (r.zero()) return number();
  if (r == half()) throw not_defined{};
  if (r == quarter(1)) return number(rational(1));
  if (r == quarter(3)) return number(rational(-1));
  if (r == sixth(1)) return number(rational(1) / rational(3), factor::sqrt_of{rational(3)});
  if (r == sixth(2)) return number(rational(1), factor::sqrt_of{rational(3)});
  if (r == sixth(4)) return number(rational(-1), factor::sqrt_of{rational(3)});
  if (r == sixth(5)) return number(rational(-1) / rational(3), factor::sqrt_of{rational(3)});
  return number(rational(1), factor::tan_pi_of{r});
}

// x as a rational multiple of pi, where it is one.
struct pi_part {
  const rational &coefficient;
  std::optional<rational> operator()(const factor::pi &) const { return coefficient; }
  template <class F> std::optional<rational> operator()(const F &) const {
    return coefficient.zero() ? std::optional<rational>(rational()) : std::nullopt;
  }
};
[[nodiscard]] inline std::optional<rational> pi_multiple(const number &x) { return std::visit(pi_part{x.coefficient}, x.factor); }
[[nodiscard]] inline number sin(const number &x) {
  if (const auto q = pi_multiple(x)) return sin_pi(*q);
  return number(rational(1), factor::other{sin(real_of(x))});
}
[[nodiscard]] inline number cos(const number &x) {
  if (const auto q = pi_multiple(x)) return cos_pi(*q);
  return number(rational(1), factor::other{cos(real_of(x))});
}
[[nodiscard]] inline number tan(const number &x) {
  if (const auto q = pi_multiple(x)) return tan_pi(*q);
  const real c = cos(real_of(x));
  try {
    (void)c.msd();
  } catch (const too_close_to_zero &) {
    throw not_defined{};
  }
  return number(rational(1), factor::other{sin(real_of(x)) / c});
}

[[nodiscard]] inline number pi_times(const rational &q) { return number(q, factor::pi{}); }
// Whether x is c sqrt(t).
struct root_test {
  const number &x;
  std::int64_t t;
  const rational &c;
  bool operator()(const factor::sqrt_of &s) const { return s.of == rational(t) && x.coefficient == c; }
  template <class F> bool operator()(const F &) const { return false; }
};
[[nodiscard]] inline bool is_root_of(const number &x, std::int64_t t, const rational &c) {
  return std::visit(root_test{x, t, c}, x.factor);
}

[[nodiscard]] inline number asin(const number &x) {
  if (const auto r = exact(x)) {
    if (r->numerator() > r->denominator()) throw not_defined{};
    if (r->zero()) return number();
    if (*r == half()) return pi_times(sixth(1));
    if (*r == -half()) return pi_times(sixth(-1));
    if (*r == rational(1)) return pi_times(half());
    if (*r == rational(-1)) return pi_times(-half());
    return number(rational(1), factor::asin_of{*r});
  }
  if (is_root_of(x, 2, half())) return pi_times(quarter(1));
  if (is_root_of(x, 2, -half())) return pi_times(quarter(-1));
  if (is_root_of(x, 3, half())) return pi_times(sixth(2));
  if (is_root_of(x, 3, -half())) return pi_times(sixth(-2));
  const real v = real_of(x);
  try {
    if ((real(1) - v * v).sign() < 0) throw not_defined{};
  } catch (const too_close_to_zero &) {
    throw not_defined{};
  }
  return number(rational(1), factor::other{asin(v)});
}
[[nodiscard]] inline number acos(const number &x) { return pi_times(half()) - asin(x); }
[[nodiscard]] inline number atan(const number &x) {
  if (const auto r = exact(x)) {
    if (r->zero()) return number();
    if (*r == rational(1)) return pi_times(quarter(1));
    if (*r == rational(-1)) return pi_times(quarter(-1));
    return number(rational(1), factor::atan_of{*r});
  }
  if (is_root_of(x, 3, rational(1))) return pi_times(sixth(2));
  if (is_root_of(x, 3, rational(-1))) return pi_times(sixth(-2));
  if (is_root_of(x, 3, rational(1) / rational(3))) return pi_times(sixth(1));
  if (is_root_of(x, 3, rational(-1) / rational(3))) return pi_times(sixth(-1));
  return number(rational(1), factor::other{atan(real_of(x))});
}

// A power whose exact value would be too large to keep, refused before it
// is worked out.
struct too_large : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Value too large"; }
};

// x^y: an integer power by squaring (exact where the factors multiply by
// rule); a half power a square root; a positive base otherwise e^(y ln x).
// A rational to an integer power stays exact, whatever the power -- and is
// refused where its size, its base's bits times the power, is past what is
// kept.
[[nodiscard]] inline number pow(const number &x, const number &y) {
  const auto k = exact(y).and_then([](const rational &q) { return small_integer(q); });
  if (k)
    if (const auto base = exact(x); base && !base->zero() && base->bits() > 2 &&
                                     static_cast<double>(base->bits()) * static_cast<double>(std::abs(*k)) > double(1 << 18))
      throw too_large{};
  if (k && (exact(x) || std::abs(*k) <= 4096)) {
    if (*k == 0) return number(rational(1));
    if (zero(x)) {
      if (*k < 0) throw division_by_zero_t{};
      return number();
    }
    number base = *k < 0 ? reciprocal(x) : x;
    number out(rational(1));
    for (std::int64_t n = std::abs(*k); n > 0; n >>= 1) {
      if (n & 1) out = out * base;
      if (n > 1) base = base * base;
    }
    return out;
  }
  if (const auto q = exact(y); q && *q == half()) return sqrt(x);
  if (zero(x)) {
    if (sign(y) > 0) return number();
    throw not_defined{};
  }
  if (sign(x) < 0) throw not_defined{};
  return exp(y * ln(x));
}

// Degrees and radians: an angle in degrees is (d / 180) pi.
[[nodiscard]] inline number radians_of_degrees(const number &degrees) { return degrees * pi_times(rational(1) / rational(180)); }
[[nodiscard]] inline number degrees_of_radians(const number &radians) {
  if (const auto q = pi_multiple(radians)) return number(*q * rational(180));
  return number(rational(1), factor::other{real_of(radians) * real(180) / pi()});
}

// ---- digits --------------------------------------------------------------------

// The digits of a number that is not rational: the first `count`, each
// right; a real that cannot be told from zero, none.
[[nodiscard]] inline std::optional<decimal_digits> digits(const number &x, std::size_t count) {
  try {
    return digits_of(real_of(x), count);
  } catch (const too_close_to_zero &) {
    return std::nullopt;
  }
}

}  // namespace calculator
