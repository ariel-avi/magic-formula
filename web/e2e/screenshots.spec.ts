import { fileURLToPath } from 'node:url';
import { expect, test } from '@playwright/test';
import { curves, openExample } from './support';

test('captures the workbench in light and dark', async ({ browser, baseURL }) => {
  for (const theme of ['light', 'dark'] as const) {
    const context = await browser.newContext({
      baseURL,
      viewport: { width: 1600, height: 1000 },
      colorScheme: theme,
    });
    const page = await context.newPage();
    await page.addInitScript((t) => localStorage.setItem('mf-theme', t), theme);
    await openExample(page);
    await expect(curves(page)).toHaveCount(3);
    await page.evaluate(() => document.fonts.ready.then(() => undefined));
    await page.screenshot({
      fullPage: true,
      path: fileURLToPath(new URL(`../test-results/screenshots/workbench-${theme}.png`, import.meta.url)),
    });
    await context.close();
  }
});
