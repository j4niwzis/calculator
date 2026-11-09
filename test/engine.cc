// SPDX-License-Identifier: AGPL-3.0-only
// The calculator's keys pressed as a hand presses them -- a formula typed,
// its value shown under it, = -- and the numbers under it.
import std;
import gtest;
import calculator.engine;
import calculator.real;

#include "gtest/gtest-macros.h"

namespace {
using namespace calculator;

// Keys pressed in order, from a fresh calculator.
template <class... Keys> calc keys(Keys... pressed) {
  calc c;
  ((c = press(std::move(c), pressed)), ...);
  return c;
}
// A formula typed character by character: digits and the point, the four
// operations, ^, parentheses (one key for both, as Android has), !, %.
calc type(calc c, std::string_view text) {
  for (const char one : text) {
    switch (one) {
      case '.': c = press(std::move(c), point{}); break;
      case '+': c = press(std::move(c), choose{op::add{}}); break;
      case '-': c = press(std::move(c), choose{op::subtract{}}); break;
      case '*': c = press(std::move(c), choose{op::multiply{}}); break;
      case '/': c = press(std::move(c), choose{op::divide{}}); break;
      case '^': c = press(std::move(c), choose{op::power{}}); break;
      case '(': case ')': c = press(std::move(c), parentheses{}); break;
      case '!': c = press(std::move(c), after{post::factorial{}}); break;
      case '%': c = press(std::move(c), after{post::percent{}}); break;
      case 'p': c = press(std::move(c), put{constant::pi{}}); break;
      case 'e': c = press(std::move(c), put{constant::e{}}); break;
      default: c = press(std::move(c), digit{one - '0'}); break;
    }
  }
  return c;
}
calc type(std::string_view text) { return type(calc{}, text); }
// Typed, then =: the number shown.
std::string result(std::string_view text) {
  const calc c = press(type(text), equals{});
  return c.failed ? std::string(message(*c.failed)) : formula_text(c);
}

// ---- a formula, typed --------------------------------------------------------

TEST(Formula, StartsEmpty) { EXPECT_EQ(formula_text(calc{}), ""); }

TEST(Formula, ShownAsTyped) { EXPECT_EQ(formula_text(type("12+7*3")), "12+7×3"); }

TEST(Formula, NumbersGroupedByThousands) { EXPECT_EQ(formula_text(type("1234567+1")), "1,234,567+1"); }

TEST(Formula, TheResultUnderItAsItIsTyped) { EXPECT_EQ(result_text(type("12+7")), "19"); }

TEST(Formula, NoResultUnderANumberAlone) { EXPECT_EQ(result_text(type("12")), ""); }

TEST(Formula, NoResultUnderAnOperationWithNothingAfterIt) { EXPECT_EQ(result_text(type("12+")), ""); }

TEST(Formula, AnOperationReplacesTheOneBeforeIt) { EXPECT_EQ(formula_text(type("9+*4")), "9×4"); }

TEST(Formula, AMinusAfterTimesIsASign) { EXPECT_EQ(result("2*-3"), "−6"); }

TEST(Formula, AMinusFirstIsASign) { EXPECT_EQ(result("-5+2"), "−3"); }

TEST(Formula, NoOtherOperationFirst) { EXPECT_EQ(formula_text(type("*")), ""); }

TEST(Formula, ASecondPointIsIgnored) { EXPECT_EQ(formula_text(type("1..2")), "1.2"); }

TEST(Formula, ALeadingPoint) { EXPECT_EQ(formula_text(type(".5")), "0.5"); }

TEST(Formula, LeadingZerosDropped) { EXPECT_EQ(formula_text(type("007")), "7"); }

TEST(Formula, AtMostTwentyDigits) { EXPECT_EQ(formula_text(type(std::string(25, '1'))), "11,111,111,111,111,111,111"); }

TEST(Formula, BackspaceTakesADigit) { EXPECT_EQ(formula_text(press(type("123"), backspace{})), "12"); }

TEST(Formula, BackspaceTakesAFunctionWithItsParenthesis) {
  EXPECT_EQ(formula_text(press(keys(digit{2}, choose{op::add{}}, of{fn::sine{}}), backspace{})), "2+");
}

TEST(Formula, ClearClearsEverything) { EXPECT_EQ(press(type("5+3"), clear{}).formula.size(), 0u); }

// ---- precedence --------------------------------------------------------------

TEST(Precedence, TimesBeforePlus) { EXPECT_EQ(result("2+3*4"), "14"); }

TEST(Precedence, LeftToRightAtOneLevel) { EXPECT_EQ(result("20-5-3"), "12"); }

TEST(Precedence, PowerToTheRight) { EXPECT_EQ(result("2^3^2"), "512"); }

TEST(Precedence, PowerBeforeTheSign) { EXPECT_EQ(result("-2^2"), "−4"); }

TEST(Precedence, Parentheses) { EXPECT_EQ(result("(2+3)*4"), "20"); }

TEST(Precedence, AParenthesisLeftOpenIsClosedAtTheEnd) { EXPECT_EQ(result("2*(3+4"), "14"); }

TEST(Precedence, TheParenthesisKeyClosesWhereOneIsOpen) { EXPECT_EQ(formula_text(type("(2+3)")), "(2+3)"); }

TEST(Precedence, AProductWrittenSideBySide) { EXPECT_EQ(result("3(4+1)"), "15"); }

TEST(Precedence, TwoPi) { EXPECT_EQ(result("2p"), "6.28318530717958647692528676655900576839433879875021164194988…"); }

TEST(Precedence, FactorialAndPercent) {
  EXPECT_EQ(result("5!"), "120");
  EXPECT_EQ(result("50%"), "0.5");
  EXPECT_EQ(result("200*5%"), "10");
}

TEST(Precedence, AnUnfinishedFormulaIsABadExpression) { EXPECT_EQ(result("2+"), "Bad expression"); }

// ---- exact ------------------------------------------------------------------

TEST(Exact, ATenthAndAFifth) { EXPECT_EQ(result("0.1+0.2"), "0.3"); }

TEST(Exact, AThirdTimesThree) { EXPECT_EQ(result("1/3*3"), "1"); }

TEST(Exact, AHugeNumberPlusOneLessItself) { EXPECT_EQ(result("10^100+1-10^100"), "1"); }

TEST(Exact, ALongProduct) { EXPECT_EQ(result("123456789*987654321"), "121,932,631,112,635,269"); }

TEST(Exact, AThirdShownCut) {
  EXPECT_EQ(result("1/3"), "0.333333333333333333333333333333333333333333333333333333333333…");
}

TEST(Exact, TheResultKeptWholeThoughShownCut) {
  // 1/3 shown cut, kept whole: x 3 goes on from it and is 1.
  calc c = press(type("1/3"), equals{});
  c = press(type(std::move(c), "*3"), equals{});
  EXPECT_EQ(formula_text(c), "1");
}

TEST(Exact, ADigitAfterAResultStartsAgain) {
  calc c = press(type("1+1"), equals{});
  EXPECT_EQ(formula_text(type(std::move(c), "4")), "4");
}

// ---- failures --------------------------------------------------------------------

TEST(Failures, DivisionByZero) { EXPECT_EQ(result("7/0"), "Can't divide by 0"); }

TEST(Failures, TheFormulaKeptAfterAFailure) {
  const calc c = press(type("7/0"), equals{});
  EXPECT_EQ(formula_text(c), "7÷0");
  EXPECT_EQ(result_text(c), "Can't divide by 0");
}

TEST(Failures, NothingShownUnderAFormulaWithNoValue) { EXPECT_EQ(result_text(type("7/0")), ""); }

TEST(Failures, TooLarge) { EXPECT_EQ(result("10^99999*10^99999"), "Value too large"); }

TEST(Failures, TheFactorialOfAFraction) { EXPECT_EQ(result("2.5!"), "Not defined"); }

TEST(Failures, WhatEachSays) {
  EXPECT_STREQ(division_by_zero{}.what(), "Can't divide by 0");
  EXPECT_STREQ(out_of_range{}.what(), "Value too large");
}

// ---- the scientific keys ----------------------------------------------------------

TEST(Scientific, Pi) { EXPECT_EQ(result("p"), "3.14159265358979323846264338327950288419716939937510582097494…"); }

TEST(Scientific, TheSquareRootOfTwoSquared) {
  EXPECT_EQ(formula_text(press(press(keys(of{fn::square_root{}}, digit{2}, parentheses{}), after{post::squared{}}), equals{})), "2");
}

TEST(Scientific, SinOfThirtyDegrees) {
  EXPECT_EQ(formula_text(press(keys(switch_angles{}, of{fn::sine{}}, digit{3}, digit{0}), equals{})), "0.5");
}

TEST(Scientific, SinOfASixthOfPi) {
  calc c = keys(of{fn::sine{}});
  c = press(type(std::move(c), "p/6"), equals{});
  EXPECT_EQ(formula_text(c), "0.5");
}

TEST(Scientific, AsinOfAHalfInDegrees) {
  calc c = keys(switch_angles{}, of{fn::arcsine{}});
  c = press(type(std::move(c), "0.5"), equals{});
  EXPECT_EQ(formula_text(c), "30");
}

TEST(Scientific, LnOfE) {
  calc c = keys(of{fn::natural_log{}}, put{constant::e{}});
  EXPECT_EQ(formula_text(press(std::move(c), equals{})), "1");
}

TEST(Scientific, TenToTheThird) {
  calc c = keys(of{fn::ten_to{}}, digit{3});
  EXPECT_EQ(formula_text(press(std::move(c), equals{})), "1,000");
}

TEST(Scientific, TheLnOfANegativeNumber) {
  calc c = keys(of{fn::natural_log{}});
  EXPECT_EQ(result_text(press(type(std::move(c), "-2"), equals{})), "Not defined");
}

TEST(Scientific, TheAnglesShown) { EXPECT_EQ(angles(keys(switch_angles{})), "DEG"); }

TEST(Scientific, TheInverseKeys) { EXPECT_TRUE(keys(switch_inverse{}).inverse); }

// ---- the numbers below: rationals, reals, the symbolic layer ------------------------

TEST(Number, LongDivision) {
  const natural a = natural::from_decimal("123456789012345678901234567890");
  const natural b = natural::from_decimal("98765432109876543210");
  const auto [q, rest] = divmod(a * b + natural::from_decimal("12345"), b);
  EXPECT_EQ(q.decimal(), a.decimal());
  EXPECT_EQ(rest.decimal(), "12345");
}

TEST(Number, LowestTerms) { EXPECT_EQ(rational::from_decimal("0.50"), rational(1) / rational(2)); }

TEST(Number, Ordered) {
  EXPECT_LT(rational(1) / rational(3), rational(1) / rational(2));
  EXPECT_LT(rational(-1), rational(1) / rational(2));
  EXPECT_LT(rational(-2), rational(-1));
}

std::string first(const real &x, std::size_t count) {
  const decimal_digits d = digits_of(x, count);
  return std::format("{}{}e{}", d.negative ? "-" : "", d.digits, d.exponent);
}

TEST(Real, Pi) { EXPECT_EQ(first(pi(), 30), "314159265358979323846264338327e0"); }
TEST(Real, E) { EXPECT_EQ(first(exp(real(1)), 30), "271828182845904523536028747135e0"); }
TEST(Real, SquareRootOfTwo) { EXPECT_EQ(first(sqrt(real(2)), 30), "141421356237309504880168872420e0"); }
TEST(Real, LnOfTwo) { EXPECT_EQ(first(ln(real(2)), 30), "693147180559945309417232121458e-1"); }
TEST(Real, SinOfOne) { EXPECT_EQ(first(sin(real(1)), 30), "841470984807896506652502321630e-1"); }
TEST(Real, AtanOfOne) { EXPECT_EQ(first(atan(real(1)), 30), "785398163397448309615660845819e-1"); }
TEST(Real, ANegativeSine) { EXPECT_EQ(first(sin(real(4)), 20), "-75680249530792825137e-1"); }
TEST(Real, ATinyExponential) { EXPECT_EQ(first(exp(real(-50)), 18), "192874984796391778e-22"); }

number q(std::int64_t n, std::int64_t d = 1) { return number(rational(n) / rational(d)); }
const number pi_number(rational(1), factor::pi{});

TEST(Symbolic, PiPlusThreePiIsFourPi) { EXPECT_EQ(pi_number + q(3) * pi_number, number(rational(4), factor::pi{})); }
TEST(Symbolic, TheSquareRootOfEight) { EXPECT_EQ(sqrt(q(8)), number(rational(2), factor::sqrt_of{rational(2)})); }
TEST(Symbolic, SinOfAQuarterOfPi) {
  EXPECT_EQ(sin(q(1, 4) * pi_number), number(rational(1) / rational(2), factor::sqrt_of{rational(2)}));
}
TEST(Symbolic, LnFourLessTwiceLnTwo) { EXPECT_EQ(ln(q(4)) - q(2) * ln(q(2)), number()); }
TEST(Symbolic, ExpOfLnTwo) { EXPECT_EQ(exp(ln(q(2))), q(2)); }
TEST(Symbolic, LogOfAThousand) { EXPECT_EQ(log10(q(1000)), q(3)); }
TEST(Symbolic, AtanOfOne) { EXPECT_EQ(atan(q(1)), number(rational(1) / rational(4), factor::pi{})); }
TEST(Symbolic, TanOfHalfPi) { EXPECT_THROW((void)tan(q(1, 2) * pi_number), not_defined); }
TEST(Symbolic, NineToTheHalf) { EXPECT_EQ(pow(q(9), q(1, 2)), q(3)); }
}  // namespace
