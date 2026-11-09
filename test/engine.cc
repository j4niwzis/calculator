// SPDX-License-Identifier: AGPL-3.0-only
// The calculator's keys pressed as a hand presses them, and what it shows.
import std;
import gtest;
import calculator.engine;

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

TEST(Failures, DivisionByZeroThrows) { EXPECT_THROW((void)apply(op::divide{}, 1.0, 0.0), division_by_zero); }

TEST(Failures, AResultTooLargeThrows) { EXPECT_THROW((void)apply(op::multiply{}, 1e308, 10.0), out_of_range); }

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

TEST(Entry, AtMostFifteenDigits) { EXPECT_EQ(display(type("1111111111111111111")), "111,111,111,111,111"); }

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

TEST(Written, TwelveSignificantDigits) { EXPECT_EQ(written(1.0 / 3.0), "0.333333333333"); }

TEST(Written, ALargeExponent) { EXPECT_EQ(written(1e16), "1e16"); }

TEST(Written, ASmallExponent) { EXPECT_EQ(written(1e-7), "1e-7"); }

TEST(Written, NoNegativeZero) { EXPECT_EQ(written(apply(op::multiply{}, -1.0, 0.0)), "0"); }
}  // namespace
