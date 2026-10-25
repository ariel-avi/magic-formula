import { expect, test } from '@playwright/test';
import { MAX_SWEEPS } from '../src/domain/chart-spec';
import { commit, curves, dashes, inputRow, openExample } from './support';

test('starts with one sweep that cannot be removed', async ({ page }) => {
  await openExample(page);
  await expect(page.getByTestId('sweep-tab')).toHaveCount(1);
  await expect(page.getByTestId('sweep-tab')).toHaveText('Sweep 1');
  await expect(page.getByTestId('sweep-remove')).toBeDisabled();
});

test('duplicates the selected sweep into the same chart', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await expect(page.getByTestId('sweep-tab')).toHaveCount(2);
  await expect(page.getByRole('tab', { name: 'Sweep 2', exact: true })).toHaveAttribute('aria-selected', 'true');
  await expect(curves(page)).toHaveCount(6);
  await expect(page.getByTestId('legend-group')).toHaveCount(2);
  await expect(page.getByTestId('legend-item')).toHaveCount(6);
  const d = await dashes(page);
  expect(d.slice(0, 3)).toEqual(['', '', '']);
  const last = d.slice(3);
  expect(last[0]).not.toBe('');
  expect(last).toEqual([last[0], last[0], last[0]]);
});

test('solves each sweep independently', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await page.getByRole('tab', { name: 'Table' }).click();
  const first = page.locator('[data-testid="series-table-section"][data-sweep="sweep-1"]');
  const second = page.locator('[data-testid="series-table-section"][data-sweep="sweep-2"]');
  const firstBefore = await first.textContent();
  const secondBefore = await second.textContent();
  await commit(page.getByTestId('sweep-end'), '10');
  await expect.poll(() => second.textContent()).not.toBe(secondBefore);
  expect(await first.textContent()).toBe(firstBefore);
});

test('keeps roles per sweep', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await inputRow(page, 'SR').getByRole('radio', { name: 'Sweep' }).click();
  await expect(inputRow(page, 'SR')).toHaveAttribute('data-role', 'sweep');
  await page.getByRole('tab', { name: 'Sweep 1', exact: true }).click();
  await expect(inputRow(page, 'SA')).toHaveAttribute('data-role', 'sweep');
  await expect(page.getByTestId('x-axis-select')).toContainText('SA');
});

test('adds, renames, switches to pure slip and removes a sweep', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-add').click();
  await expect(page.getByTestId('sweep-tab')).toHaveCount(2);
  await page.getByTestId('sweep-name').fill('Pure slip');
  await expect(page.getByRole('tab', { name: 'Pure slip' })).toBeVisible();
  await expect(page.getByTestId('legend-group-name').filter({ hasText: 'Pure slip' })).toHaveCount(1);
  await page.getByTestId('uncombined-switch').click();
  await expect(page.getByTestId('uncombined-switch')).toHaveAttribute('aria-checked', 'true');
  await page.getByTestId('sweep-remove').click();
  await expect(page.getByTestId('sweep-tab')).toHaveCount(1);
  await expect(page.getByTestId('sweep-remove')).toBeDisabled();
});

test('limits the chart to MAX_SWEEPS sweeps', async ({ page }) => {
  await openExample(page);
  for (let n = 2; n <= MAX_SWEEPS; n += 1) {
    await page.getByTestId('sweep-duplicate').click();
    await expect(page.getByTestId('sweep-tab')).toHaveCount(n);
  }
  await expect(page.getByTestId('sweep-tab')).toHaveCount(MAX_SWEEPS);
  await expect(page.getByTestId('sweep-add')).toBeDisabled();
  await expect(page.getByTestId('sweep-duplicate')).toBeDisabled();
});

test('reports problems per sweep while the others still plot', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await commit(page.getByTestId('sweep-points'), '1');
  await expect(page.getByTestId('chart-problems')).toBeVisible();
  await expect(page.getByTestId('chart-problems')).toContainText('Sweep 2');
  await expect(curves(page)).toHaveCount(3);
});

test('shows one table section per sweep', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await page.getByRole('tab', { name: 'Table' }).click();
  const tables = page.getByTestId('series-table');
  await expect(tables).toHaveCount(2);
  for (const table of await tables.all()) {
    await expect(table.locator('tbody tr')).toHaveCount(61);
  }
});

test('hints when a sweep does not vary along X', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('sweep-duplicate').click();
  await inputRow(page, 'SR').getByRole('radio', { name: 'Sweep' }).click();
  await expect(page.getByTestId('flat-sweeps')).toContainText('Sweep 2');
  await expect(page.getByTestId('flat-sweeps')).toContainText('SR');
});

test('draws a full-width dash swatch in every sweep tab', async ({ page }) => {
  await openExample(page);
  for (let n = 2; n <= MAX_SWEEPS; n += 1) {
    await page.getByTestId('sweep-duplicate').click();
    await expect(page.getByTestId('sweep-tab')).toHaveCount(n);
  }
  const widths = await page
    .getByTestId('sweep-tab')
    .locator('svg')
    .evaluateAll((svgs) => svgs.map((svg) => svg.getBoundingClientRect().width));
  expect(widths).toEqual(Array(MAX_SWEEPS).fill(32));
});
