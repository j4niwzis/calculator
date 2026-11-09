// SPDX-License-Identifier: AGPL-3.0-only
// The calculator's keys pressed as a hand presses them, and what it shows.
import std;
import calculator.engine;

using namespace calculator;

namespace {
int failures = 0;

void check(std::string_view what, std::string_view got, std::string_view want) {
  const bool ok = got == want;
  std::println("{} {}{}", ok ? "ok  " : "FAIL", what, ok ? std::string() : std::format(": got \"{}\", want \"{}\"", got, want));
  if (!ok) ++failures;
}

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

const choose plus{op::add{}};
const choose minus{op::subtract{}};
const choose times{op::multiply{}};
const choose over{op::divide{}};
}  // namespace

int main() {
  check("starts at 0", display(calc{}), "0");
  check("12 + 7 =", display(keys(digit{1}, digit{2}, plus, digit{7}, equals{})), "19");
  check("0.1 + 0.2 =", display(press(type(press(type(calc{}, "0.1"), plus), "0.2"), equals{})), "0.3");
  check("2 + 3 = = repeats + 3", display(keys(digit{2}, plus, digit{3}, equals{}, equals{})), "8");
  check("5 + = takes 5 as the right side", display(keys(digit{5}, plus, equals{})), "10");
  check("2 + 3 x 4 = works left to right", display(keys(digit{2}, plus, digit{3}, times, digit{4}, equals{})), "20");
  check("9 + - 4 = takes the last operation", display(keys(digit{9}, plus, minus, digit{4}, equals{})), "5");
  check("the expression while waiting", expression(keys(digit{1}, digit{2}, plus)), "12 +");
  check("the expression worked out", expression(keys(digit{1}, digit{2}, plus, digit{7}, equals{})), "12 + 7 =");
  check("a new number after = starts over", expression(keys(digit{1}, plus, digit{1}, equals{}, digit{4})), "");

  check("7 / 0 =", display(keys(digit{7}, over, digit{0}, equals{})), "Cannot divide by zero");
  check("a digit after a failure starts again", display(keys(digit{7}, over, digit{0}, equals{}, digit{3})), "3");
  check("an operation after a failure is refused", display(keys(digit{7}, over, digit{0}, equals{}, plus)), "Cannot divide by zero");
  {
    const outcome big = apply(op::multiply{}, 1e308, 10.0);
    check("1e308 x 10 is out of range", big ? std::string("a number") : std::string(message(big.error())), "Out of range");
  }

  check("grouped by thousands", display(type(calc{}, "1234567")), "1,234,567");
  check("its fraction not grouped", display(type(calc{}, "1234.5678")), "1,234.5678");
  check("a leading point", display(keys(point{}, digit{5})), "0.5");
  check("a second point ignored", display(type(calc{}, "1..2")), "1.2");
  check("at most 15 digits", display(type(calc{}, "1111111111111111111")), "111,111,111,111,111");
  check("leading zeros dropped", display(type(calc{}, "007")), "7");

  check("200 + 5% is 200 + 10", display(press(press(type(press(type(calc{}, "200"), plus), "5"), percent{}), equals{})), "210");
  check("50 x 10% is 50 x 0.1", display(press(press(type(press(type(calc{}, "50"), times), "10"), percent{}), equals{})), "5");
  check("a percent alone", display(press(type(calc{}, "42"), percent{})), "0.42");

  check("negated", display(keys(digit{5}, negate{})), "-5");
  check("negated twice", display(keys(digit{5}, negate{}, negate{})), "5");
  check("5 - (-3) =", display(keys(digit{5}, minus, negate{}, digit{3}, equals{})), "8");
  check("a negative grouped", display(press(type(calc{}, "1234567"), negate{})), "-1,234,567");

  check("backspace", display(press(type(calc{}, "123"), backspace{})), "12");
  check("backspace to 0", display(keys(digit{1}, backspace{}, backspace{})), "0");
  check("no backspace into a result", display(keys(digit{1}, digit{2}, plus, digit{3}, equals{}, backspace{})), "15");
  check("CE keeps the left side", display(keys(digit{5}, plus, digit{3}, clear_entry{}, digit{1}, equals{})), "6");
  check("C clears everything", display(keys(digit{5}, plus, digit{3}, clear{})), "0");

  check("written: 12 digits", written(1.0 / 3.0), "0.333333333333");
  check("written: a large exponent", written(1e16), "1e16");
  check("written: a small exponent", written(1e-7), "1e-7");
  check("written: no negative zero", written(*apply(op::multiply{}, -1.0, 0.0)), "0");

  std::println("{} failed", failures);
  return failures == 0 ? 0 : 1;
}
