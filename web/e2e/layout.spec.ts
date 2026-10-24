import { expect, test, type Page } from '@playwright/test';
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
