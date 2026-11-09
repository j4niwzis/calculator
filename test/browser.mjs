// SPDX-License-Identifier: AGPL-3.0-only
// The published page, pressed in real browsers as Android's calculator is:
// a formula typed by the mouse and by the keyboard, its result under it,
// = making it the number.
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const playwright = require('playwright');

const url = process.argv[2];
// The display's lines: the angles, a spacer, the formula, its result.
// The formula: three lines of it, one a size, all the same text (one
// shown as its length says): the first read.
const formula = (page) => page.locator('[data-view=Display] > :nth-child(3)').innerText();
const below = (page) => page.locator('[data-view=Display] > :nth-child(6)').innerText();
const click = (page, label) => page.getByRole('button', {name: label, exact: true}).first().click();
const until = (page, line, text) => page.waitForFunction(([n, t]) => document.querySelector('[data-view=Display] > :nth-child(' + n + ')').innerText === t, [line, text]);

for (const kind of ['chromium', 'firefox']) {
  const browser = await playwright[kind].launch();
  try {
    const page = await browser.newPage({viewport: {width: 412, height: 915}});
    const errors = [];
    page.on('pageerror', (error) => errors.push(error.message));
    page.on('console', (message) => console.log(`[${message.type()}] ${message.text()}`));
    await page.goto(url, {waitUntil: 'load'});
    // Taken over by the WebAssembly: a key changes the formula.
    const deadline = Date.now() + 60000;
    for (;;) {
      await click(page, '1');
      if ((await formula(page)) !== '') break;
      if (Date.now() > deadline) {
        console.log(errors.join('\n'));
        console.log(await page.locator('[data-view=Display]').evaluate((e) => e.outerHTML));
        throw new Error('the page was never taken over by the WebAssembly');
      }
      await page.waitForTimeout(500);
    }
    await click(page, 'AC');

    for (const label of ['1', '2', '+', '7']) await click(page, label);
    assert.equal(await formula(page), '12+7');
    await until(page, 4, '19');
    await click(page, '=');
    await until(page, 3, '19');

    await click(page, 'AC');
    for (const label of ['7', '÷', '0', '=']) await click(page, label);
    await until(page, 4, "Can't divide by 0");

    // The keyboard: Escape clears, what is typed is the formula, Enter works it out.
    await page.keyboard.press('Escape');
    await page.keyboard.type('2+3*4');
    await page.keyboard.press('Enter');
    await until(page, 3, '14');
    await page.keyboard.press('Escape');
    await page.keyboard.type('1234*3');
    await page.keyboard.press('Enter');
    await until(page, 3, '3,702');
    await page.keyboard.press('Escape');
    await page.keyboard.type('0.1+0.2=');
    await until(page, 3, '0.3');

    assert.deepEqual(errors, []);
    console.log(`${kind}: ok`);
  } finally {
    await browser.close();
  }
}
