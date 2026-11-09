// SPDX-License-Identifier: AGPL-3.0-only
// Constructive real numbers: a real is what can say itself to any precision
// asked -- approx(p), an integer n with |x - n * 2^p| < 2^p -- each answer
// kept, so that a coarser one is the finer cut down. The arithmetic and the
// functions are Hans Boehm's (the "recursive reals" of his calculator;
// Boehm, Cartwright, Riggle, O'Donnell, "Exact real arithmetic: a case study
// in higher order programming", and his CR library), as nodes of a tree:
// each kind of node one alternative of a variant, its approximation an
// overload.
//
// A real can say whether it is above or below a number only up to the
// precision asked: zero is never known to be zero. What decides that is the
// layer above (calculator.symbolic), which keeps a number exact where it can.
export module calculator.real;

import std;
import calculator.number;

export namespace calculator {

class real;

namespace node {
struct of_rational {
  rational value;
};
struct sum {
  std::shared_ptr<const struct holder> a, b;
};
struct negation {
  std::shared_ptr<const holder> a;
};
struct product {
  std::shared_ptr<const holder> a, b;
};
struct inverse {
  std::shared_ptr<const holder> a;
};
// x * 2^by
struct shift {
  std::shared_ptr<const holder> a;
  int by = 0;
};
struct square_root {
  std::shared_ptr<const holder> a;
};
// exp(x), |x| at most about 1/256
struct prescaled_exp {
  std::shared_ptr<const holder> a;
};
// ln(1 + x), |x| below 1/2
struct prescaled_ln {
  std::shared_ptr<const holder> a;
};
// cos(x), |x| below 1
struct prescaled_cos {
  std::shared_ptr<const holder> a;
};
// atan(x), |x| at most 5/16
struct prescaled_atan {
  std::shared_ptr<const holder> a;
};
// atan(1/n), n an integer above 1
struct integral_atan {
  std::int64_t n = 2;
};
using kind = std::variant<of_rational, sum, negation, product, inverse, shift, square_root, prescaled_exp, prescaled_ln,
                          prescaled_cos, prescaled_atan, integral_atan>;

// A node and the finest approximation it was asked for.
struct holder {
  kind what;
  mutable bool valid = false;
  mutable int min_prec = 0;
  mutable integer max_appr;
};
}  // namespace node

// Asked for more precision than any display needs: a number this close to
// zero is taken as one that cannot be told from it.
struct too_close_to_zero : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Too close to zero to tell"; }
};

inline constexpr int kUnknownMsd = std::numeric_limits<int>::min();
// The finest precision a real is asked for while finding where its first
// bit is: some 1,200 decimal digits.
inline constexpr int kFinestPrecision = -4000;

namespace detail {
// k * 2^n, rounded to the nearest.
[[nodiscard]] inline integer scale(const integer &k, int n) {
  if (n >= 0) return k.shifted(n);
  return (k.shifted(n + 1) + integer(1)).shifted(-1);
}
// ceil(log2(|n| + 1)): how many bits n takes.
[[nodiscard]] inline int bound_log2(int n) {
  return static_cast<int>(std::bit_width(static_cast<unsigned>(std::abs(n)) + 0u));
}
[[nodiscard]] inline integer approx(const node::holder &h, int p);
}  // namespace detail

class real {
 public:
  real() : real(rational()) {}
  explicit real(const rational &value) : node_(make(node::of_rational{value})) {}
  explicit real(std::int64_t value) : real(rational(value)) {}

  // An integer n with |x - n * 2^p| < 2^p.
  [[nodiscard]] integer approx(int p) const;

  // Where its most significant bit is, msd: 2^(msd-1) < |x| < 2^(msd+1); or
  // kUnknownMsd, where it is within 2^p of zero.
  [[nodiscard]] int msd(int p) const;
  // The same, asked at finer and finer precision down to p.
  [[nodiscard]] int iter_msd(int p) const;
  // Where its most significant bit is, which must be known: a real that
  // cannot be told from zero by kFinestPrecision throws.
  [[nodiscard]] int msd() const {
    const int found = iter_msd(kFinestPrecision);
    if (found == kUnknownMsd) throw too_close_to_zero{};
    return found;
  }
  // Its sign, found by asking finer and finer; a real too close to zero
  // throws.
  [[nodiscard]] int sign() const;

  friend real operator+(const real &a, const real &b) { return real(make(node::sum{a.node_, b.node_})); }
  friend real operator-(const real &a) { return real(make(node::negation{a.node_})); }
  friend real operator-(const real &a, const real &b) { return a + -b; }
  friend real operator*(const real &a, const real &b) { return real(make(node::product{a.node_, b.node_})); }
  // b must not be zero (its msd is found).
  friend real operator/(const real &a, const real &b) { return a * b.inverse(); }
  [[nodiscard]] real inverse() const { return real(make(node::inverse{node_})); }
  [[nodiscard]] real shifted(int by) const { return real(make(node::shift{node_, by})); }

  // The same real: one node, shared.
  friend bool operator==(const real &a, const real &b) { return a.node_ == b.node_; }

  // Its node, and a real made of a node: for the functions below.
  [[nodiscard]] const std::shared_ptr<const node::holder> &node() const noexcept { return node_; }
  [[nodiscard]] static real of(node::kind what) { return real(make(std::move(what))); }

 private:
  std::shared_ptr<const node::holder> node_;

  explicit real(std::shared_ptr<const node::holder> n) : node_(std::move(n)) {}
  [[nodiscard]] static std::shared_ptr<const node::holder> make(node::kind what) {
    return std::make_shared<const node::holder>(node::holder{std::move(what)});
  }
};

namespace detail {
[[nodiscard]] inline int msd_of(const node::holder &h, int p) {
  if (!h.valid || h.max_appr.abs() <= integer(1)) {
    (void)approx(h, p - 1);
    if (h.max_appr.abs() <= integer(1)) return kUnknownMsd;
  }
  return h.min_prec + static_cast<int>(h.max_appr.bits()) - 1;
}
[[nodiscard]] inline int known_msd_of(const node::holder &h) { return h.min_prec + static_cast<int>(h.max_appr.bits()) - 1; }
[[nodiscard]] inline int iter_msd_of(const node::holder &h, int p) {
  for (int prec = 0; prec > p + 30; prec = (prec * 3) / 2 - 16) {
    if (const int found = msd_of(h, prec); found != kUnknownMsd) return found;
  }
  return msd_of(h, p);
}

// round(q * 2^-p)
[[nodiscard]] inline integer approximate(const node::of_rational &x, int p) {
  integer n = x.value.signed_numerator();
  integer d(false, x.value.denominator());
  if (p <= 0)
    n = n.shifted(-p);
  else
    d = d.shifted(p);
  // Rounded to the nearest: (2n + d) / 2d, rounded down.
  const integer twice = n.shifted(1) + d;
  const integer twice_d = d.shifted(1);
  integer q = twice / twice_d;
  if (twice.negative() && q * twice_d != twice) q = q - integer(1);
  return q;
}
[[nodiscard]] inline integer approximate(const node::sum &x, int p) {
  return scale(approx(*x.a, p - 2) + approx(*x.b, p - 2), -2);
}
[[nodiscard]] inline integer approximate(const node::negation &x, int p) { return -approx(*x.a, p); }
[[nodiscard]] inline integer approximate(const node::shift &x, int p) { return approx(*x.a, p - x.by); }
[[nodiscard]] inline integer approximate(const node::product &x, int p) {
  const node::holder *op1 = x.a.get();
  const node::holder *op2 = x.b.get();
  const int half_prec = (p >> 1) - 1;
  int msd_op1 = msd_of(*op1, half_prec);
  if (msd_op1 == kUnknownMsd) {
    const int msd_op2 = msd_of(*op2, half_prec);
    if (msd_op2 == kUnknownMsd) return integer(0);
    std::swap(op1, op2);
    msd_op1 = msd_op2;
  }
  const int prec2 = p - msd_op1 - 3;
  const integer appr2 = approx(*op2, prec2);
  if (appr2.zero()) return integer(0);
  const int msd_op2 = known_msd_of(*op2);
  const int prec1 = p - msd_op2 - 3;
  const integer appr1 = approx(*op1, prec1);
  return scale(appr1 * appr2, prec1 + prec2 - p);
}
[[nodiscard]] inline integer approximate(const node::inverse &x, int p) {
  const int msd = iter_msd_of(*x.a, kFinestPrecision);
  if (msd == kUnknownMsd) throw too_close_to_zero{};
  const int inv_msd = 1 - msd;
  const int digits_needed = inv_msd - p + 3;
  const int prec_needed = msd - digits_needed;
  const int log_scale_factor = -p - prec_needed;
  if (log_scale_factor < 0) return integer(0);
  const integer dividend = integer(1).shifted(log_scale_factor);
  const integer scaled_divisor = approx(*x.a, prec_needed);
  const integer abs_divisor = scaled_divisor.abs();
  const integer result = (dividend + abs_divisor.shifted(-1)) / abs_divisor;
  return scaled_divisor.negative() ? -result : result;
}
// sqrt(x) * 2^-(p-2) from x * 2^-2(p-2): the integer square root, which is
// within 2 of it; then rounded to p.
[[nodiscard]] inline integer approximate(const node::square_root &x, int p) {
  const integer a = approx(*x.a, 2 * (p - 2));
  if (a.negative() || a.zero()) return integer(0);
  return scale(integer(false, a.magnitude().square_root()), -2);
}
[[nodiscard]] inline integer approximate(const node::prescaled_exp &x, int p) {
  if (p >= 1) return integer(0);
  const int iterations_needed = -p / 2 + 2;
  const int calc_precision = p - bound_log2(2 * iterations_needed) - 4;
  const int op_prec = p - 3;
  const integer op_appr = approx(*x.a, op_prec);
  const integer scaled_1 = integer(1).shifted(-calc_precision);
  integer current_term = scaled_1;
  integer current_sum = scaled_1;
  std::int64_t n = 0;
  const integer max_trunc_error = integer(1).shifted(p - 4 - calc_precision);
  while (current_term.abs() >= max_trunc_error) {
    ++n;
    current_term = scale(current_term * op_appr, op_prec) / integer(n);
    current_sum = current_sum + current_term;
  }
  return scale(current_sum, calc_precision - p);
}
[[nodiscard]] inline integer approximate(const node::prescaled_ln &x, int p) {
  if (p >= 0) return integer(0);
  const int iterations_needed = -p;
  const int calc_precision = p - bound_log2(2 * iterations_needed) - 4;
  const int op_prec = p - 3;
  const integer op_appr = approx(*x.a, op_prec);
  integer x_nth = scale(op_appr, op_prec - calc_precision);
  integer current_term = x_nth;
  integer current_sum = current_term;
  std::int64_t n = 1;
  std::int64_t current_sign = 1;
  const integer max_trunc_error = integer(1).shifted(p - 4 - calc_precision);
  while (current_term.abs() >= max_trunc_error) {
    ++n;
    current_sign = -current_sign;
    x_nth = scale(x_nth * op_appr, op_prec);
    current_term = x_nth / integer(n * current_sign);
    current_sum = current_sum + current_term;
  }
  return scale(current_sum, calc_precision - p);
}
[[nodiscard]] inline integer approximate(const node::prescaled_cos &x, int p) {
  if (p >= 1) return integer(0);
  const int iterations_needed = -p / 2 + 4;
  const int calc_precision = p - bound_log2(2 * iterations_needed) - 4;
  const int op_prec = p - 2;
  const integer op_appr = approx(*x.a, op_prec);
  const integer max_trunc_error = integer(1).shifted(p - 4 - calc_precision);
  std::int64_t n = 0;
  integer current_term = integer(1).shifted(-calc_precision);
  integer current_sum = current_term;
  while (current_term.abs() >= max_trunc_error) {
    n += 2;
    current_term = scale(current_term * op_appr, op_prec);
    current_term = scale(current_term * op_appr, op_prec);
    current_term = current_term / integer(-n * (n - 1));
    current_sum = current_sum + current_term;
  }
  return scale(current_sum, calc_precision - p);
}
// x - x^3/3 + x^5/5 - ...: each term at least three bits below the last.
[[nodiscard]] inline integer approximate(const node::prescaled_atan &x, int p) {
  if (p >= 1) return integer(0);
  const int iterations_needed = -p / 3 + 2;
  const int calc_precision = p - bound_log2(2 * iterations_needed) - 4;
  const int op_prec = p - 3;
  const integer op_appr = approx(*x.a, op_prec);
  integer x_nth = scale(op_appr, op_prec - calc_precision);
  integer current_term = x_nth;
  integer current_sum = x_nth;
  std::int64_t n = 1;
  std::int64_t current_sign = 1;
  const integer max_trunc_error = integer(1).shifted(p - 4 - calc_precision);
  while (current_term.abs() >= max_trunc_error) {
    n += 2;
    current_sign = -current_sign;
    x_nth = scale(x_nth * op_appr, op_prec);
    x_nth = scale(x_nth * op_appr, op_prec);
    current_term = x_nth / integer(n * current_sign);
    current_sum = current_sum + current_term;
  }
  return scale(current_sum, calc_precision - p);
}
[[nodiscard]] inline integer approximate(const node::integral_atan &x, int p) {
  if (p >= 1) return integer(0);
  const int iterations_needed = -p / 2 + 2;
  const int calc_precision = p - bound_log2(2 * iterations_needed) - 2;
  const integer scaled_1 = integer(1).shifted(-calc_precision);
  const integer big_op(x.n);
  const integer big_op_squared = big_op * big_op;
  const integer op_inverse = scaled_1 / big_op;
  integer current_power = op_inverse;
  integer current_term = op_inverse;
  integer current_sum = op_inverse;
  std::int64_t current_sign = 1;
  std::int64_t n = 1;
  const integer max_trunc_error = integer(1).shifted(p - 2 - calc_precision);
  while (current_term.abs() >= max_trunc_error) {
    n += 2;
    current_power = current_power / big_op_squared;
    current_sign = -current_sign;
    current_term = current_power / integer(current_sign * n);
    current_sum = current_sum + current_term;
  }
  return scale(current_sum, calc_precision - p);
}

[[nodiscard]] inline integer approx(const node::holder &h, int p) {
  if (h.valid && p >= h.min_prec) return scale(h.max_appr, h.min_prec - p);
  integer result = std::visit([&](const auto &one) { return approximate(one, p); }, h.what);
  h.valid = true;
  h.min_prec = p;
  h.max_appr = result;
  return result;
}
}  // namespace detail

// ---- a real's own, through its node ---------------------------------------

inline integer real::approx(int p) const { return detail::approx(*node_, p); }
inline int real::msd(int p) const { return detail::msd_of(*node_, p); }
inline int real::iter_msd(int p) const { return detail::iter_msd_of(*node_, p); }
inline int real::sign() const {
  (void)msd();
  return node_->max_appr.sign();
}

// ---- the functions, as Boehm reduces them to the series above -------------

// Thrown for what has no real value here: the logarithm of a number not
// above zero.
struct not_positive : std::exception {
  [[nodiscard]] const char *what() const noexcept override { return "Not a positive number"; }
};

inline real sqrt(const real &x) { return real::of(node::square_root{x.node()}); }

// pi = 16 atan(1/5) - 4 atan(1/239) (Machin).
inline real pi() {
  static const real kept = (real::of(node::integral_atan{5}).shifted(2) - real::of(node::integral_atan{239})).shifted(2);
  return kept;
}
// ln 2 = 7 ln(10/9) - 2 ln(25/24) + 3 ln(81/80).
inline real ln2() {
  static const real kept = real(7) * real::of(node::prescaled_ln{real(rational(1) / rational(9)).node()}) -
                           real(2) * real::of(node::prescaled_ln{real(rational(1) / rational(24)).node()}) +
                           real(3) * real::of(node::prescaled_ln{real(rational(1) / rational(80)).node()});
  return kept;
}

// exp(x): halved until small, the series there, squared back.
inline real exp(const real &x) {
  const integer rough = x.approx(-10);
  if (rough > integer(2) || rough < integer(-2)) {
    const real half = exp(x.shifted(-1));
    return half * half;
  }
  return real::of(node::prescaled_exp{x.node()});
}

// ln(x), x above zero: brought near 1 -- inverted below 1/2, rooted or
// halved above 3/2 -- and ln(1 + y) there.
inline real ln(const real &x) {
  const integer rough = x.approx(-4);  // in sixteenths
  if (rough < integer(0)) throw not_positive{};
  if (rough <= integer(8)) return -ln(x.inverse());
  if (rough >= integer(24)) {
    if (rough <= integer(64)) return ln(sqrt(sqrt(x))).shifted(2);
    const int extra_bits = static_cast<int>(rough.bits()) - 3;
    return ln(x.shifted(-extra_bits)) + real(extra_bits) * ln2();
  }
  return real::of(node::prescaled_ln{(x - real(1)).node()});
}

// cos(x): its multiples of pi taken off, halved while above 1, the series.
inline real cos(const real &x) {
  const integer halfpi_multiples = (x / pi()).approx(-1);
  if (halfpi_multiples.abs() >= integer(2)) {
    const integer pi_multiples = detail::scale(halfpi_multiples, -1);
    const real adjustment = pi() * real(rational(pi_multiples));
    const bool odd = (pi_multiples.magnitude().low() & 1u) != 0;
    return odd ? -cos(x - adjustment) : cos(x - adjustment);
  }
  if (x.approx(-1).abs() >= integer(2)) {
    const real half = cos(x.shifted(-1));
    return (half * half).shifted(1) - real(1);
  }
  return real::of(node::prescaled_cos{x.node()});
}
inline real sin(const real &x) { return cos(pi().shifted(-1) - x); }
inline real tan(const real &x) { return sin(x) / cos(x); }

// atan(x): above 2, pi/2 less atan(1/x); above 5/16, the half angle,
// 2 atan(x / (1 + sqrt(1 + x^2))); the series below.
inline real atan(const real &x) {
  const integer rough = x.approx(-4);  // in sixteenths
  if (rough > integer(32) || rough < integer(-32)) {
    const real half_pi = pi().shifted(-1);
    const real rest = atan(x.inverse());
    return rough.negative() ? -half_pi - rest : half_pi - rest;
  }
  if (rough > integer(4) || rough < integer(-4)) return atan(x / (real(1) + sqrt(real(1) + x * x))).shifted(1);
  return real::of(node::prescaled_atan{x.node()});
}
// asin(x) = atan(x / sqrt(1 - x^2)), |x| below 1; acos(x) = pi/2 - asin(x).
inline real asin(const real &x) { return atan(x / sqrt(real(1) - x * x)); }
inline real acos(const real &x) { return pi().shifted(-1) - asin(x); }

// ---- a real's decimal digits --------------------------------------------------

// |x| = d.ddd... x 10^exponent: its first digits, each one right (cut, never
// rounded); and its sign.
struct decimal_digits {
  bool negative = false;
  std::string digits;
  int exponent = 0;
  friend bool operator==(const decimal_digits &, const decimal_digits &) = default;
};

[[nodiscard]] inline rational power_of_ten(int k) {
  natural ten_to(1);
  for (int i = 0; i < std::abs(k); ++i) ten_to.multiply_add(10, 0);
  return k >= 0 ? rational(false, std::move(ten_to), natural(1)) : rational(false, natural(1), std::move(ten_to));
}

// floor(|x|), where |x| is at least 1: from approximations finer and finer
// until the floor of everything within one of them is the same integer. One
// never told apart (x an integer the arithmetic cannot see is one) is taken
// as the integer it is next to.
[[nodiscard]] inline integer floor_of_magnitude(const real &x, bool &negative) {
  constexpr int kFinestGuard = 4000;
  for (int guard = 8;; guard += 32) {
    const integer a = x.approx(-guard);
    negative = a.negative();
    const integer m = a.abs();
    const integer low = (m - integer(1)).shifted(-guard);
    const integer high = (m + integer(1)).shifted(-guard);
    if (low == high) return low;
    if (guard >= kFinestGuard) return high;
  }
}

// The first `count` significant digits of x, x not zero (a real too close to
// zero to tell throws too_close_to_zero).
[[nodiscard]] inline decimal_digits digits_of(const real &x, std::size_t count) {
  const int msd = x.msd();
  int exponent = static_cast<int>(std::floor((msd - 1) * 0.30102999566398120));
  const integer lowest = integer(rational(power_of_ten(static_cast<int>(count) - 1)).signed_numerator());
  const integer above = integer(rational(power_of_ten(static_cast<int>(count))).signed_numerator());
  for (int tries = 0; tries < 8; ++tries) {
    const int k = static_cast<int>(count) - 1 - exponent;
    bool negative = false;
    const integer n = floor_of_magnitude(x * real(power_of_ten(k)), negative);
    if (n < lowest) {
      --exponent;
      continue;
    }
    if (n >= above) {
      ++exponent;
      continue;
    }
    return {negative, n.magnitude().decimal(), exponent};
  }
  throw too_close_to_zero{};
}

}  // namespace calculator
