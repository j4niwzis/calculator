// SPDX-License-Identifier: AGPL-3.0-only
// The published page, pressed in real browsers: by the mouse, then by the
// keyboard, and what the display says after each.
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const playwright = require('playwright');

const url = process.argv[2];
const shown = (page) => page.locator('[data-view=Display] > :last-child').innerText();
const above = (page) => page.locator('[data-view=Display] > :nth-child(2)').innerText();
const click = (page, label) => page.getByRole('button', {name: label, exact: true}).click();

for (const kind of ['chromium', 'firefox']) {
  const browser = await playwright[kind].launch();
  try {
    const page = await browser.newPage({viewport: {width: 420, height: 760}});
    const errors = [];
    page.on('pageerror', (error) => errors.push(error.message));
    await page.goto(url, {waitUntil: 'load'});
    // Taken over by the WebAssembly: a key changes the display.
    const deadline = Date.now() + 60000;
    for (;;) {
      await click(page, '1');
      if ((await shown(page)) === '1') break;
      if (Date.now() > deadline) throw new Error('the page was never taken over by the WebAssembly');
      await page.waitForTimeout(500);
    }
    await click(page, 'C');

    for (const label of ['1', '2', '+', '7']) await click(page, label);
    assert.equal(await above(page), '12 +');
    await click(page, '=');
    await page.waitForFunction(() => document.querySelector('[data-view=Display] > :last-child').innerText === '19');
    assert.equal(await above(page), '12 + 7 =');

    await click(page, 'C');
    for (const label of ['7', '÷', '0', '=']) await click(page, label);
    assert.equal(await shown(page), 'Cannot divide by zero');

    // The keyboard: Escape clears, digits and operators type, Enter works it out.
    await page.keyboard.press('Escape');
    await page.keyboard.type('1234*3');
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => document.querySelector('[data-view=Display] > :last-child').innerText === '3,702');
    await page.keyboard.press('Escape');
    await page.keyboard.type('0.1+0.2=');
    assert.equal(await shown(page), '0.3');
    await page.keyboard.type('5');
    await page.keyboard.press('Backspace');
    assert.equal(await shown(page), '0');

    assert.deepEqual(errors, []);
    console.log(`${kind}: ok`);
  } finally {
    await browser.close();
  }
}
