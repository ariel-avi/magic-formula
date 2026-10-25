import { expect, test, type Page } from '@playwright/test';
import { MAX_SWEEPS } from '../src/domain/chart-spec';
import { openExample } from './support';

const noHorizontalScroll = (page: Page) =>
  page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth);

for (const [width, height] of [
  [1600, 1000],
  [2560, 1440],
]) {
  test(`spans the full viewport width at ${width}x${height}`, async ({ page }) => {
    await page.setViewportSize({ width, height });
    await openExample(page);
    const shell = await page.getByTestId('app-shell').boundingBox();
    const chart = await page.getByTestId('chart-pane').boundingBox();
    expect(Math.abs((shell?.width ?? 0) - width)).toBeLessThanOrEqual(1);
    expect(chart?.width ?? 0).toBeGreaterThan(0.4 * width);
    expect(await noHorizontalScroll(page)).toBe(true);
  });
}

test('stacks the panes without horizontal scrolling on a phone', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await openExample(page);
  expect(await noHorizontalScroll(page)).toBe(true);
  let previous = -1;
  for (const id of ['inputs-pane', 'chart-pane', 'model-pane']) {
    const y = (await page.getByTestId(id).boundingBox())?.y ?? Number.NaN;
    expect(y).toBeGreaterThan(previous);
    previous = y;
  }
});

test('keeps six long-named sweeps inside the phone width', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await openExample(page);
  for (let n = 2; n <= MAX_SWEEPS; n += 1) {
    await page.getByTestId('sweep-duplicate').click();
    await expect(page.getByTestId('sweep-tab')).toHaveCount(n);
  }
  await page.getByTestId('sweep-name').fill('W'.repeat(24));
  expect(await noHorizontalScroll(page)).toBe(true);
  await page.getByRole('tab', { name: 'Table' }).click();
  await expect(page.getByTestId('series-table')).toHaveCount(MAX_SWEEPS);
  expect(await noHorizontalScroll(page)).toBe(true);
  await page.getByRole('tab', { name: 'Plot' }).click();
  expect(await noHorizontalScroll(page)).toBe(true);
});
