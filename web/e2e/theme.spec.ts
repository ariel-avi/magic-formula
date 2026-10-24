import { expect, test } from '@playwright/test';
import { SERIES_PALETTES, toCss } from '../src/domain/colours';
import { openExample, strokes } from './support';

const stored = (page: import('@playwright/test').Page) => page.evaluate(() => localStorage.getItem('mf-theme'));

test('follows the system colour scheme until the user chooses', async ({ page }) => {
  await page.emulateMedia({ colorScheme: 'dark' });
  await page.goto('/');
  await expect(page.locator('html')).toHaveClass(/dark/);
  await page.emulateMedia({ colorScheme: 'light' });
  await page.reload();
  await expect(page.locator('html')).not.toHaveClass(/dark/);
});

test('switches theme from the header and keeps the choice after reload', async ({ page }) => {
  await page.emulateMedia({ colorScheme: 'light' });
  await page.goto('/');
  const toggle = page.getByTestId('theme-toggle');
  await toggle.click();
  await expect(page.locator('html')).toHaveClass(/dark/);
  expect(await stored(page)).toBe('dark');
  await expect(toggle).toHaveAttribute('aria-label', 'Use light theme');
  await page.reload();
  await expect(page.locator('html')).toHaveClass(/dark/);
  await page.getByTestId('theme-toggle').click();
  await expect(page.locator('html')).not.toHaveClass(/dark/);
  expect(await stored(page)).toBe('light');
});

test('uses the dark series palette in the dark theme', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('mf-theme', 'dark'));
  await openExample(page);
  await expect.poll(() => strokes(page)).toEqual([0, 1, 2].map((i) => toCss(SERIES_PALETTES.dark[i])));
});
