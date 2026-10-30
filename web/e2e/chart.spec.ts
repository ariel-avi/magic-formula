import { expect, test } from '@playwright/test';
import { SERIES_PALETTES, toCss } from '../src/domain/colours';
import { curves, inputRow, openExample, parseOklch, strokes } from './support';

test('draws one line per step combination', async ({ page }) => {
  await openExample(page);
  await inputRow(page, 'IA').getByRole('radio', { name: 'Step' }).click();
  const add = page.getByTestId('step-add-IA');
  await add.fill('2');
  await add.press('Enter');
  await expect(curves(page)).toHaveCount(6);
  await expect(page.getByTestId('legend-item')).toHaveCount(6);
  const colours = await page
    .getByTestId('legend-item')
    .evaluateAll((items) => items.map((item) => item.getAttribute('data-colour') ?? ''));
  expect([...colours].sort()).toEqual([...(await strokes(page))].sort());
});

test('colours lines by hue or by saturation', async ({ page }) => {
  await openExample(page);
  await expect(curves(page)).toHaveCount(3);
  const palette = SERIES_PALETTES.light;
  const hueStrokes = await strokes(page);
  expect(hueStrokes).toEqual([0, 1, 2].map((i) => toCss(palette[i])));
  expect(new Set(hueStrokes.map((s) => parseOklch(s).h)).size).toBe(3);

  await page.getByTestId('colour-mode-FZ').getByRole('radio', { name: 'Saturation' }).click();
  await expect.poll(async () => new Set((await strokes(page)).map((s) => parseOklch(s).h)).size).toBe(1);
  const parsed = (await strokes(page)).map(parseOklch);
  for (const colour of parsed) {
    expect(colour.h).toBeCloseTo(palette[0].h, 1);
    expect(colour.l).toBeCloseTo(parsed[0].l, 3);
  }
  expect(parsed[0].c).toBeGreaterThan(parsed[1].c);
  expect(parsed[1].c).toBeGreaterThan(parsed[2].c);
});

test('plots the chosen Y channel', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('y-axis-select').click();
  await page.getByRole('option', { name: /^FX \[N\]/ }).click();
  await expect(page.getByTestId('sweep-chart')).toContainText('FX [N]');
});

test('labels the X axis with round tick values including zero', async ({ page }) => {
  await openExample(page);
  const ticks = page.getByTestId('sweep-chart').locator('.recharts-xAxis-tick-labels text');
  await expect(ticks.first()).toBeVisible();
  const labels = (await ticks.allTextContents()).map((text) => text.replace('−', '-'));
  expect(labels).toEqual(expect.arrayContaining(['-10', '0', '10']));
  expect(labels).not.toContain('-7');
});

test('shows the plotted values as a table', async ({ page }) => {
  await openExample(page);
  await page.getByRole('tab', { name: 'Table' }).click();
  const table = page.getByTestId('series-table');
  await expect(table.locator('tbody tr')).toHaveCount(61);
  await expect(table.locator('thead th')).toHaveCount(4);
});
