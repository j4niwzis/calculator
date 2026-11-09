// SPDX-License-Identifier: AGPL-3.0-only
// The calculator's keys pressed as a hand presses them, and what it shows.
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
// A number typed, digit by digit.
calc type(calc c, std::string_view number) {
  for (const char one : number) {
    if (one == '.')
      c = press(std::move(c), point{});
    else
      c = press(std::move(c), digit{one - '0'});
  }
  return c;
}
calc type(std::string_view number) { return type(calc{}, number); }

const choose plus{op::add{}};
const choose minus{op::subtract{}};
const choose times{op::multiply{}};
const choose over{op::divide{}};

TEST(Arithmetic, StartsAtZero) { EXPECT_EQ(display(calc{}), "0"); }

TEST(Arithmetic, AddsWhatIsTyped) { EXPECT_EQ(display(keys(digit{1}, digit{2}, plus, digit{7}, equals{})), "19"); }

TEST(Arithmetic, RoundsWhatADoubleCannotHold) {
  EXPECT_EQ(display(press(type(press(type("0.1"), plus), "0.2"), equals{})), "0.3");
}

TEST(Arithmetic, EqualsAgainRepeatsTheLastOperation) {
  EXPECT_EQ(display(keys(digit{2}, plus, digit{3}, equals{}, equals{})), "8");
}

TEST(Arithmetic, EqualsWithNoRightSideTakesTheLeftSide) { EXPECT_EQ(display(keys(digit{5}, plus, equals{})), "10"); }

TEST(Arithmetic, WorksLeftToRight) {
  EXPECT_EQ(display(keys(digit{2}, plus, digit{3}, times, digit{4}, equals{})), "20");
}

TEST(Arithmetic, ALaterOperationReplacesAnEarlierOne) {
  EXPECT_EQ(display(keys(digit{9}, plus, minus, digit{4}, equals{})), "5");
}

TEST(Expression, ShowsTheOperationWaiting) { EXPECT_EQ(expression(keys(digit{1}, digit{2}, plus)), "12 +"); }

TEST(Expression, ShowsWhatWasWorkedOut) {
  EXPECT_EQ(expression(keys(digit{1}, digit{2}, plus, digit{7}, equals{})), "12 + 7 =");
}

TEST(Expression, ANewNumberAfterEqualsStartsOver) {
  EXPECT_EQ(expression(keys(digit{1}, plus, digit{1}, equals{}, digit{4})), "");
}

TEST(Failures, DivisionByZeroThrows) {
  EXPECT_THROW((void)apply(op::divide{}, number(rational(1)), number(rational(0))), division_by_zero);
}

TEST(Failures, AResultTooLargeThrows) {
  const rational big = rational::from_decimal("1" + std::string(20000, '0'));
  EXPECT_THROW((void)apply(op::multiply{}, number(big), number(big)), out_of_range);
}

TEST(Failures, WhatEachSays) {
  EXPECT_STREQ(division_by_zero{}.what(), "Cannot divide by zero");
  EXPECT_STREQ(out_of_range{}.what(), "Out of range");
}

TEST(Failures, AKeyCatchesWhatItsArithmeticThrows) {
  calc failed;
  EXPECT_NO_THROW(failed = keys(digit{7}, over, digit{0}, equals{}));
  EXPECT_EQ(display(failed), "Cannot divide by zero");
}

TEST(Failures, ADigitStartsAgain) { EXPECT_EQ(display(keys(digit{7}, over, digit{0}, equals{}, digit{3})), "3"); }

TEST(Failures, AnOperationIsRefused) {
  EXPECT_EQ(display(keys(digit{7}, over, digit{0}, equals{}, plus)), "Cannot divide by zero");
}

TEST(Entry, GroupedByThousands) { EXPECT_EQ(display(type("1234567")), "1,234,567"); }

TEST(Entry, ItsFractionIsNotGrouped) { EXPECT_EQ(display(type("1234.5678")), "1,234.5678"); }

TEST(Entry, ALeadingPoint) { EXPECT_EQ(display(keys(point{}, digit{5})), "0.5"); }

TEST(Entry, ASecondPointIsIgnored) { EXPECT_EQ(display(type("1..2")), "1.2"); }

TEST(Entry, AtMostTwentyDigits) { EXPECT_EQ(display(type(std::string(25, '1'))), "11,111,111,111,111,111,111"); }

TEST(Entry, LeadingZerosAreDropped) { EXPECT_EQ(display(type("007")), "7"); }

TEST(Percent, BesidePlusIsAPercentOfTheLeftSide) {
  EXPECT_EQ(display(press(press(type(press(type("200"), plus), "5"), percent{}), equals{})), "210");
}

TEST(Percent, BesideTimesIsAHundredth) {
  EXPECT_EQ(display(press(press(type(press(type("50"), times), "10"), percent{}), equals{})), "5");
}

TEST(Percent, Alone) { EXPECT_EQ(display(press(type("42"), percent{})), "0.42"); }

TEST(Sign, Negated) { EXPECT_EQ(display(keys(digit{5}, negate{})), "-5"); }

TEST(Sign, NegatedTwice) { EXPECT_EQ(display(keys(digit{5}, negate{}, negate{})), "5"); }

TEST(Sign, ANegativeRightSide) { EXPECT_EQ(display(keys(digit{5}, minus, negate{}, digit{3}, equals{})), "8"); }

TEST(Sign, ANegativeIsGrouped) { EXPECT_EQ(display(press(type("1234567"), negate{})), "-1,234,567"); }

TEST(Editing, Backspace) { EXPECT_EQ(display(press(type("123"), backspace{})), "12"); }

TEST(Editing, BackspaceToZero) { EXPECT_EQ(display(keys(digit{1}, backspace{}, backspace{})), "0"); }

TEST(Editing, NoBackspaceIntoAResult) {
  EXPECT_EQ(display(keys(digit{1}, digit{2}, plus, digit{3}, equals{}, backspace{})), "15");
}

TEST(Editing, ClearEntryKeepsTheLeftSide) {
  EXPECT_EQ(display(keys(digit{5}, plus, digit{3}, clear_entry{}, digit{1}, equals{})), "6");
}

TEST(Editing, ClearClearsEverything) { EXPECT_EQ(keys(digit{5}, plus, digit{3}, clear{}), calc{}); }

rational ten_to(std::size_t n) { return rational::from_decimal("1" + std::string(n, '0')); }

TEST(Written, ARepeatingDecimalIsCutNotRounded) {
  EXPECT_EQ(written(rational(1) / rational(3)), "0.33333333333333333333\u2026");
  EXPECT_EQ(written(rational(2) / rational(3)), "0.66666666666666666666\u2026");
}

TEST(Written, ATerminatingDecimalIsExact) { EXPECT_EQ(written(rational(1) / rational(8)), "0.125"); }

TEST(Written, ALargeExactPower) { EXPECT_EQ(written(ten_to(30)), "1e30"); }

TEST(Written, ALargeNumberCut) {
  EXPECT_EQ(written(rational::from_decimal("123456789012345678901234567")), "1.2345678901234567890\u2026e26");
}

TEST(Written, SmallWithoutAnExponent) { EXPECT_EQ(written(rational(1) / ten_to(7)), "0.0000001"); }

TEST(Written, ASmallExponent) { EXPECT_EQ(written(rational(1) / ten_to(30)), "1e-30"); }

TEST(Written, NoNegativeZero) { EXPECT_EQ(written(apply(op::multiply{}, number(rational(-1)), number(rational(0)))), "0"); }

TEST(Exact, AHugeNumberPlusOneMinusItself) {
  EXPECT_EQ(written(apply(op::subtract{}, apply(op::add{}, number(ten_to(100)), number(rational(1))), number(ten_to(100)))), "1");
}

TEST(Exact, AThirdTimesThreeIsOne) { EXPECT_EQ(display(keys(digit{1}, over, digit{3}, times, digit{3}, equals{})), "1"); }

TEST(Exact, AThirdShownCut) { EXPECT_EQ(display(keys(digit{1}, over, digit{3}, equals{})), "0.33333333333333333333\u2026"); }

TEST(Exact, AResultKeptWholeThoughShownCut) {
  // 1/3 shown cut, kept whole: x 3 again is 1, not 0.99999...
  EXPECT_EQ(display(keys(digit{1}, over, digit{3}, equals{}, times, digit{3}, equals{})), "1");
}

TEST(Exact, ALongProduct) {
  EXPECT_EQ(display(press(type(press(type("123456789"), times), "987654321"), equals{})), "121,932,631,112,635,269");
}

TEST(Number, LongDivision) {
  const natural a = natural::from_decimal("123456789012345678901234567890");
  const natural b = natural::from_decimal("98765432109876543210");
  const natural r = natural::from_decimal("12345");
  const auto [q, rest] = divmod(a * b + r, b);
  EXPECT_EQ(q.decimal(), a.decimal());
  EXPECT_EQ(rest.decimal(), "12345");
}

TEST(Number, DecimalRoundTrip) {
  const std::string digits = "340282366920938463463374607431768211456";
  EXPECT_EQ(natural::from_decimal(digits).decimal(), digits);
}

TEST(Number, LowestTerms) { EXPECT_EQ(rational::from_decimal("0.50"), rational(1) / rational(2)); }
// A real's first digits and the exponent of the first: d...e<exponent>.
std::string first(const real &x, std::size_t count) {
  const decimal_digits d = digits_of(x, count);
  return std::format("{}{}e{}", d.negative ? "-" : "", d.digits, d.exponent);
}

TEST(Real, Pi) { EXPECT_EQ(first(pi(), 30), "314159265358979323846264338327e0"); }

TEST(Real, E) { EXPECT_EQ(first(exp(real(1)), 30), "271828182845904523536028747135e0"); }

TEST(Real, SquareRootOfTwo) { EXPECT_EQ(first(sqrt(real(2)), 30), "141421356237309504880168872420e0"); }

TEST(Real, LnOfTwo) { EXPECT_EQ(first(ln(real(2)), 30), "693147180559945309417232121458e-1"); }

TEST(Real, LnOfTen) { EXPECT_EQ(first(ln(real(10)), 30), "230258509299404568401799145468e0"); }

TEST(Real, SinOfOne) { EXPECT_EQ(first(sin(real(1)), 30), "841470984807896506652502321630e-1"); }

TEST(Real, CosOfOne) { EXPECT_EQ(first(cos(real(1)), 30), "540302305868139717400936607442e-1"); }

TEST(Real, AtanOfOneIsAQuarterOfPi) { EXPECT_EQ(first(atan(real(1)), 30), "785398163397448309615660845819e-1"); }

TEST(Real, AsinOfAHalfIsASixthOfPi) {
  EXPECT_EQ(first(asin(real(rational(1) / rational(2))), 20), "52359877559829887307e-1");
}

TEST(Real, ANegativeSine) { EXPECT_EQ(first(sin(real(4)), 20), "-75680249530792825137e-1"); }

TEST(Real, ATinyExponential) { EXPECT_EQ(first(exp(real(-50)), 18), "192874984796391778e-22"); }

TEST(Real, ALargeExponential) { EXPECT_EQ(first(exp(real(100)), 20), "26881171418161354484e43"); }

TEST(Real, LnOfANegativeNumberThrows) { EXPECT_THROW((void)ln(real(-2)), not_positive); }
// ---- the symbolic numbers: what stays exact ----------------------------------

number q(std::int64_t n, std::int64_t d = 1) { return number(rational(n) / rational(d)); }
const number pi_number(rational(1), factor::pi{});

TEST(Symbolic, PiPlusThreePiIsFourPi) { EXPECT_EQ(pi_number + q(3) * pi_number, number(rational(4), factor::pi{})); }

TEST(Symbolic, TheSquareRootOfEightIsTwoRootsOfTwo) {
  EXPECT_EQ(sqrt(q(8)), number(rational(2), factor::sqrt_of{rational(2)}));
}

TEST(Symbolic, RootTwoTimesRootTwoIsTwo) { EXPECT_EQ(sqrt(q(2)) * sqrt(q(2)), q(2)); }

TEST(Symbolic, SinOfASixthOfPiIsAHalf) { EXPECT_EQ(sin(q(1, 6) * pi_number), q(1, 2)); }

TEST(Symbolic, CosOfAThirdOfPiIsAHalf) { EXPECT_EQ(cos(q(1, 3) * pi_number), q(1, 2)); }

TEST(Symbolic, SinOfAQuarterOfPiIsHalfRootTwo) {
  EXPECT_EQ(sin(q(1, 4) * pi_number), number(rational(1) / rational(2), factor::sqrt_of{rational(2)}));
}

TEST(Symbolic, LnFourLessTwiceLnTwoIsZero) { EXPECT_EQ(ln(q(4)) - q(2) * ln(q(2)), number()); }

TEST(Symbolic, ExpOfLnTwoIsTwo) { EXPECT_EQ(exp(ln(q(2))), q(2)); }

TEST(Symbolic, LogOfAThousandIsThree) { EXPECT_EQ(log10(q(1000)), q(3)); }

TEST(Symbolic, AtanOfOneIsAQuarterOfPi) { EXPECT_EQ(atan(q(1)), number(rational(1) / rational(4), factor::pi{})); }

TEST(Symbolic, AsinOfHalfRootThreeIsAThirdOfPi) {
  EXPECT_EQ(asin(number(rational(1) / rational(2), factor::sqrt_of{rational(3)})), number(rational(1) / rational(3), factor::pi{}));
}

TEST(Symbolic, TanOfHalfPiIsNotDefined) { EXPECT_THROW((void)tan(q(1, 2) * pi_number), not_defined); }

TEST(Symbolic, TheSquareRootOfMinusOneIsNotDefined) { EXPECT_THROW((void)sqrt(q(-1)), not_defined); }

TEST(Symbolic, TwoToTheTenth) { EXPECT_EQ(pow(q(2), q(10)), q(1024)); }

TEST(Symbolic, NineToTheHalf) { EXPECT_EQ(pow(q(9), q(1, 2)), q(3)); }

// ---- the scientific keys ------------------------------------------------------

const of sine{fn::sine{}};
const of square_root{fn::square_root{}};

TEST(Scientific, PiShownWithEveryDigitRight) { EXPECT_EQ(display(keys(put{constant::pi{}})), "3.1415926535897932384…"); }

TEST(Scientific, MoreDigitsOfPi) {
  EXPECT_EQ(display(keys(put{constant::pi{}}, more_digits{})), "3.141592653589793238462643383279502884197…");
}

TEST(Scientific, TheSquareRootOfTwo) { EXPECT_EQ(display(keys(digit{2}, square_root)), "1.4142135623730950488…"); }

TEST(Scientific, RootTwoSquaredIsTwo) { EXPECT_EQ(display(keys(digit{2}, square_root, of{fn::square{}})), "2"); }

TEST(Scientific, TwoToTheTenthByKeys) {
  EXPECT_EQ(display(keys(digit{2}, choose{op::power{}}, digit{1}, digit{0}, equals{})), "1,024");
}

TEST(Scientific, SinOfThirtyDegrees) { EXPECT_EQ(display(keys(switch_angles{}, digit{3}, digit{0}, sine)), "0.5"); }

TEST(Scientific, SinOfOneRadian) { EXPECT_EQ(display(keys(digit{1}, sine)), "0.84147098480789650665…"); }

TEST(Scientific, AsinOfAHalfInDegrees) {
  EXPECT_EQ(display(keys(switch_angles{}, point{}, digit{5}, of{fn::arcsine{}})), "30");
}

TEST(Scientific, ExpOfLnTwoByKeys) { EXPECT_EQ(display(keys(digit{2}, of{fn::natural_log{}}, of{fn::exponential{}})), "2"); }

TEST(Scientific, TheReciprocalOfZero) { EXPECT_EQ(display(keys(digit{0}, of{fn::reciprocal{}})), "Cannot divide by zero"); }

TEST(Scientific, TheLnOfANegativeNumber) {
  EXPECT_EQ(display(keys(digit{2}, negate{}, of{fn::natural_log{}})), "Not defined");
}

TEST(Scientific, TheAnglesShown) { EXPECT_EQ(angles(keys(switch_angles{})), "DEG"); }
}  // namespace

