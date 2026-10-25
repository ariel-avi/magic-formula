import { expect, test } from '@playwright/test';
import { commit, curvePoints, curves, inputRow, openExample, selectAxis } from './support';

test('defaults to FY against the swept SA', async ({ page }) => {
  await openExample(page);
  await expect(page.getByTestId('x-axis-select')).toContainText('SA [deg]');
  await expect(page.getByTestId('y-axis-select')).toContainText('FY [N]');
});

test('lists inputs and outputs in groups', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('x-axis-select').click();
  const list = page.getByRole('listbox');
  await expect(list).toContainText('Inputs');
  await expect(list).toContainText('Outputs');
  await expect(page.getByRole('option', { name: /^SA \[deg\]/ })).toBeVisible();
  await expect(page.getByRole('option', { name: /^FX \[N\]/ })).toBeVisible();
  await page.keyboard.press('Escape');
});

test('moves X with the swept input', async ({ page }) => {
  await openExample(page);
  await inputRow(page, 'SR').getByRole('radio', { name: 'Sweep' }).click();
  await expect(page.getByTestId('x-axis-select')).toContainText('SR');
});

test('plots an output against an output', async ({ page }) => {
  await openExample(page);
  await commit(page.getByTestId('value-SR'), '0.1');
  await selectAxis(page, 'x', /^FX \[N\]/);
  await expect(page.getByTestId('sweep-chart')).toContainText('FX [N]');
  await expect(page.getByTestId('sweep-chart')).toContainText('FY [N]');
  await expect(curves(page)).toHaveCount(3);
});

test('connects points in sweep order when X is not monotonic', async ({ page }) => {
  await openExample(page);
  await selectAxis(page, 'y', /^SA \[deg\]/);
  await selectAxis(page, 'x', /^MZ \[N·m\]/);
  await expect(curves(page)).toHaveCount(3);
  const pts = await curvePoints(page, 0);
  expect(pts.length).toBeGreaterThan(10);
  for (let i = 1; i < pts.length; i += 1) {
    expect(pts[i].y).toBeLessThan(pts[i - 1].y);
  }
  const rising = pts.some((p, i) => i > 0 && p.x > pts[i - 1].x);
  const falling = pts.some((p, i) => i > 0 && p.x < pts[i - 1].x);
  expect(rising && falling).toBe(true);
});
