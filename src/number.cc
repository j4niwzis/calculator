// SPDX-License-Identifier: AGPL-3.0-only
// Exact numbers: unbounded naturals, and rationals made of them. Every key
// of the calculator (+ - x / % and the sign) keeps a rational rational, so
// with these its answers are exact -- 10^100 + 1 - 10^100 is 1, 1 / 3 x 3
// is 1 -- where a double answers 0 and 0.999... (Boehm, "Towards an API for
// the real numbers", the arithmetic of Android's calculator; this is its
// rational part).
export module calculator.number;

import std;

export namespace calculator {

// A natural number of any size: its 32-bit limbs, the least first, no zero
// limb last (zero has none).
class natural {
 public:
  natural() = default;
  explicit natural(std::uint64_t v) {
    for (; v != 0; v >>= 32) limbs_.push_back(static_cast<std::uint32_t>(v));
  }

  [[nodiscard]] bool zero() const noexcept { return limbs_.empty(); }
  [[nodiscard]] std::size_t bits() const noexcept {
    return limbs_.empty() ? 0 : limbs_.size() * 32 - static_cast<std::size_t>(std::countl_zero(limbs_.back()));
  }
  // Its lowest limb: the whole of it where it is under 2^32.
  [[nodiscard]] std::uint32_t low() const noexcept { return limbs_.empty() ? 0 : limbs_.front(); }

  friend bool operator==(const natural &, const natural &) = default;
  friend std::strong_ordering operator<=>(const natural &a, const natural &b) {
    if (a.limbs_.size() != b.limbs_.size()) return a.limbs_.size() <=> b.limbs_.size();
    return std::lexicographical_compare_three_way(a.limbs_.rbegin(), a.limbs_.rend(), b.limbs_.rbegin(), b.limbs_.rend());
  }

  friend natural operator+(const natural &a, const natural &b) {
    const natural &longer = a.limbs_.size() >= b.limbs_.size() ? a : b;
    const natural &shorter = a.limbs_.size() >= b.limbs_.size() ? b : a;
    natural out;
    out.limbs_.resize(longer.limbs_.size() + 1);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < longer.limbs_.size(); ++i) {
      const std::uint64_t sum = std::uint64_t{longer.limbs_[i]} + (i < shorter.limbs_.size() ? shorter.limbs_[i] : 0u) + carry;
      out.limbs_[i] = static_cast<std::uint32_t>(sum);
      carry = sum >> 32;
    }
    out.limbs_.back() = static_cast<std::uint32_t>(carry);
    out.trim();
    return out;
  }
  // a - b, b no more than a.
  friend natural operator-(const natural &a, const natural &b) {
    natural out;
    out.limbs_.resize(a.limbs_.size());
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < a.limbs_.size(); ++i) {
      const std::int64_t diff = std::int64_t{a.limbs_[i]} - (i < b.limbs_.size() ? std::int64_t{b.limbs_[i]} : 0) - borrow;
      out.limbs_[i] = static_cast<std::uint32_t>(diff);
      borrow = diff < 0 ? 1 : 0;
    }
    out.trim();
    return out;
  }
  friend natural operator*(const natural &a, const natural &b) {
    if (a.zero() || b.zero()) return {};
    natural out;
    out.limbs_.assign(a.limbs_.size() + b.limbs_.size(), 0);
    for (std::size_t i = 0; i < a.limbs_.size(); ++i) {
      std::uint64_t carry = 0;
      for (std::size_t j = 0; j < b.limbs_.size(); ++j) {
        const std::uint64_t at = std::uint64_t{a.limbs_[i]} * b.limbs_[j] + out.limbs_[i + j] + carry;
        out.limbs_[i + j] = static_cast<std::uint32_t>(at);
        carry = at >> 32;
      }
      out.limbs_[i + b.limbs_.size()] = static_cast<std::uint32_t>(carry);
    }
    out.trim();
    return out;
  }

  // The quotient and the remainder; d not zero.
  friend std::pair<natural, natural> divmod(const natural &n, const natural &d) {
    if (n < d) return {natural{}, n};
    if (d.limbs_.size() == 1) {
      natural q = n;
      const std::uint32_t r = q.divide_small(d.limbs_.front());
      return {std::move(q), natural{r}};
    }
    return long_division(n, d);
  }

  // Divided in place by d (not zero): the remainder.
  std::uint32_t divide_small(std::uint32_t d) {
    std::uint64_t rest = 0;
    for (std::size_t i = limbs_.size(); i-- > 0;) {
      const std::uint64_t at = (rest << 32) | limbs_[i];
      limbs_[i] = static_cast<std::uint32_t>(at / d);
      rest = at % d;
    }
    trim();
    return static_cast<std::uint32_t>(rest);
  }
  // Multiplied by m, plus a, in place.
  void multiply_add(std::uint32_t m, std::uint32_t a) {
    std::uint64_t carry = a;
    for (std::uint32_t &limb : limbs_) {
      const std::uint64_t at = std::uint64_t{limb} * m + carry;
      limb = static_cast<std::uint32_t>(at);
      carry = at >> 32;
    }
    if (carry != 0) limbs_.push_back(static_cast<std::uint32_t>(carry));
  }

  // From decimal digits (nothing else in them); and back.
  [[nodiscard]] static natural from_decimal(std::string_view digits) {
    natural out;
    for (const char c : digits) out.multiply_add(10, static_cast<std::uint32_t>(c - '0'));
    return out;
  }
  [[nodiscard]] std::string decimal() const {
    if (zero()) return "0";
    // Nine digits at a time, the least first; then put the right way round.
    std::vector<std::uint32_t> groups;
    for (natural rest = *this; !rest.zero();) groups.push_back(rest.divide_small(1'000'000'000u));
    std::string out = std::format("{}", groups.back());
    for (const std::uint32_t group : groups | std::views::reverse | std::views::drop(1)) out += std::format("{:09}", group);
    return out;
  }

 private:
  std::vector<std::uint32_t> limbs_;

  void trim() {
    while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
  }
  [[nodiscard]] static std::vector<std::uint32_t> shifted(const std::vector<std::uint32_t> &limbs, int by, std::size_t size) {
    std::vector<std::uint32_t> out(size, 0);
    for (std::size_t i = 0; i < limbs.size(); ++i) {
      const std::uint64_t wide = std::uint64_t{limbs[i]} << by;
      out[i] |= static_cast<std::uint32_t>(wide);
      if (i + 1 < size) out[i + 1] |= static_cast<std::uint32_t>(wide >> 32);
    }
    return out;
  }
  // Knuth's algorithm D (The Art of Computer Programming, 4.3.1), as in
  // Hacker's Delight's divmnu: d of two limbs or more.
  [[nodiscard]] static std::pair<natural, natural> long_division(const natural &n, const natural &d) {
    constexpr std::uint64_t kBase = std::uint64_t{1} << 32;
    const std::size_t dn = d.limbs_.size();
    const std::size_t m = n.limbs_.size() - dn;
    const int s = std::countl_zero(d.limbs_.back());
    const std::vector<std::uint32_t> v = shifted(d.limbs_, s, dn);
    std::vector<std::uint32_t> u = shifted(n.limbs_, s, n.limbs_.size() + 1);
    natural q;
    q.limbs_.assign(m + 1, 0);
    for (std::size_t j = m + 1; j-- > 0;) {
      const std::uint64_t top = (std::uint64_t{u[j + dn]} << 32) | u[j + dn - 1];
      std::uint64_t qhat = top / v[dn - 1];
      std::uint64_t rhat = top % v[dn - 1];
      while (qhat >= kBase || qhat * v[dn - 2] > ((rhat << 32) | u[j + dn - 2])) {
        --qhat;
        rhat += v[dn - 1];
        if (rhat >= kBase) break;
      }
      // u[j .. j+dn] -= qhat * v
      std::int64_t borrow = 0;
      std::uint64_t carry = 0;
      for (std::size_t i = 0; i < dn; ++i) {
        const std::uint64_t product = qhat * v[i] + carry;
        carry = product >> 32;
        const std::int64_t diff = std::int64_t{u[i + j]} - borrow - static_cast<std::int64_t>(product & 0xffff'ffffu);
        u[i + j] = static_cast<std::uint32_t>(diff);
        borrow = diff < 0 ? 1 : 0;
      }
      const std::int64_t last = std::int64_t{u[j + dn]} - borrow - static_cast<std::int64_t>(carry);
      u[j + dn] = static_cast<std::uint32_t>(last);
      q.limbs_[j] = static_cast<std::uint32_t>(qhat);
      // Taken once too many: added back.
      if (last < 0) {
        --q.limbs_[j];
        std::uint64_t back = 0;
        for (std::size_t i = 0; i < dn; ++i) {
          const std::uint64_t sum = std::uint64_t{u[i + j]} + v[i] + back;
          u[i + j] = static_cast<std::uint32_t>(sum);
          back = sum >> 32;
        }
        u[j + dn] += static_cast<std::uint32_t>(back);
      }
    }
    q.trim();
    // The remainder: u's low dn limbs, shifted back.
    natural r;
    r.limbs_.resize(dn);
    for (std::size_t i = 0; i < dn; ++i)
      r.limbs_[i] = s == 0 ? u[i] : static_cast<std::uint32_t>((u[i] >> s) | (std::uint64_t{u[i + 1]} << (32 - s)));
    r.trim();
    return {std::move(q), std::move(r)};
  }
};

[[nodiscard]] inline natural gcd(natural a, natural b) {
  while (!b.zero()) {
    natural r = divmod(a, b).second;
    a = std::move(b);
    b = std::move(r);
  }
  return a;
}

// A rational number, in lowest terms: its sign, numerator and denominator
// (never zero; zero is 0/1, never negative).
class rational {
 public:
  rational() : den_(1) {}
  rational(bool negative, natural num, natural den) : negative_(negative), num_(std::move(num)), den_(std::move(den)) { reduce(); }
  explicit rational(std::int64_t v) : rational(v < 0, natural(v < 0 ? 0 - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v)), natural(1)) {}

  // From a decimal as typed: an optional minus, digits, an optional point
  // and digits after it ("-12.5", "0.", "-0").
  [[nodiscard]] static rational from_decimal(std::string_view text) {
    const bool negative = text.starts_with('-');
    if (negative) text.remove_prefix(1);
    const std::size_t point = text.find('.');
    const std::string_view whole = text.substr(0, point);
    const std::string_view fraction = point == std::string_view::npos ? std::string_view() : text.substr(point + 1);
    natural den(1);
    for (std::size_t i = 0; i < fraction.size(); ++i) den.multiply_add(10, 0);
    natural num = natural::from_decimal(whole);
    for (const char c : fraction) num.multiply_add(10, static_cast<std::uint32_t>(c - '0'));
    return rational(negative, std::move(num), std::move(den));
  }

  [[nodiscard]] bool zero() const noexcept { return num_.zero(); }
  [[nodiscard]] bool negative() const noexcept { return negative_; }
  [[nodiscard]] const natural &numerator() const noexcept { return num_; }
  [[nodiscard]] const natural &denominator() const noexcept { return den_; }
  // How large it is written down: its numerator's and denominator's bits.
  [[nodiscard]] std::size_t bits() const noexcept { return num_.bits() + den_.bits(); }

  friend bool operator==(const rational &, const rational &) = default;

  [[nodiscard]] rational operator-() const { return rational(!negative_, num_, den_); }
  friend rational operator+(const rational &a, const rational &b) {
    natural x = a.num_ * b.den_;
    natural y = b.num_ * a.den_;
    natural den = a.den_ * b.den_;
    if (a.negative_ == b.negative_) return rational(a.negative_, x + y, std::move(den));
    if (x >= y) return rational(a.negative_, x - y, std::move(den));
    return rational(b.negative_, y - x, std::move(den));
  }
  friend rational operator-(const rational &a, const rational &b) { return a + -b; }
  friend rational operator*(const rational &a, const rational &b) {
    return rational(a.negative_ != b.negative_, a.num_ * b.num_, a.den_ * b.den_);
  }
  // b not zero.
  friend rational operator/(const rational &a, const rational &b) {
    return rational(a.negative_ != b.negative_, a.num_ * b.den_, a.den_ * b.num_);
  }

 private:
  bool negative_ = false;
  natural num_;
  natural den_;

  void reduce() {
    if (num_.zero()) {
      negative_ = false;
      den_ = natural(1);
      return;
    }
    const natural g = gcd(num_, den_);
    if (g != natural(1)) {
      num_ = divmod(num_, g).first;
      den_ = divmod(den_, g).first;
    }
  }
};

}  // namespace calculator
