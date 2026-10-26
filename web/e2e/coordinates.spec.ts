import { expect, test, type Page } from '@playwright/test';
import { curves, loadText, openExample, readFixture } from './support';

async function loadIso(page: Page): Promise<void> {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await loadText(page, 'pac2002_S_iso.tir', readFixture('pac2002_S_iso.tir'));
  await expect(page.getByTestId('file-name')).toHaveText('pac2002_S_iso.tir');
}

test('iso file shows the ISO scene', async ({ page }) => {
  await loadIso(page);
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
  const scene = page.getByTestId('tire-scene');
  await expect(scene).toBeVisible();
  await expect(scene).toHaveAttribute('data-system', 'ISO');
  await expect(scene.locator('canvas')).toBeVisible();
  await expect(page.getByTestId('tire-scene-label-y')).toContainText('left');
  await expect(page.getByTestId('coordinate-warnings')).toHaveCount(0);
});

test('toggling to SAE converts the model', async ({ page }) => {
  await loadIso(page);
  await page.getByTestId('coordinate-system-SAE').click();
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('SAE');
  await expect(page.getByTestId('tire-scene')).toHaveAttribute('data-system', 'SAE');
  await expect(page.getByTestId('tire-scene-label-y')).toContainText('right');
  await expect(page.getByTestId('param-row-VERTICAL-FNOMIN').getByRole('textbox')).toHaveValue('-4000');
  await expect(page.getByTestId('legend-item')).toHaveText(['FZ = -2000 N', 'FZ = -4000 N', 'FZ = -6000 N']);
  await expect(curves(page)).toHaveCount(3);
  await expect(page.getByTestId('chart-warnings')).toHaveCount(0);
  await expect(page.getByTestId('coordinate-conversion-note')).toBeVisible();
  await page.getByTestId('coordinate-system-ISO').click();
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
  await expect(page.getByTestId('tire-scene')).toHaveAttribute('data-system', 'ISO');
  await expect(page.getByTestId('tire-scene-label-y')).toContainText('left');
  await expect(page.getByTestId('param-row-VERTICAL-FNOMIN').getByRole('textbox')).toHaveValue('4000');
  await expect(page.getByTestId('legend-item')).toHaveText(['FZ = 2000 N', 'FZ = 4000 N', 'FZ = 6000 N']);
});

test('mixed file converts to ISO', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await loadText(page, 'pac2002_S.tir', readFixture('pac2002_S.tir'));
  await expect(page.getByTestId('file-name')).toHaveText('pac2002_S.tir');
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('Mixed');
  const indicators = page.getByTestId('coordinate-indicators');
  await indicators.getByRole('button', { name: 'Indicators (29)' }).click();
  await expect(page.getByTestId('coordinate-indicator-row')).toHaveCount(29);
  await expect(page.getByTestId('coordinate-system-ISO')).toHaveAttribute('data-state', 'off');
  await expect(page.getByTestId('coordinate-system-SAE')).toHaveAttribute('data-state', 'off');
  await expect(page.getByTestId('coordinate-warnings')).toContainText('mix');
  await page.getByTestId('coordinate-system-ISO').click();
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
  await expect(page.getByTestId('param-row-LATERAL_COEFFICIENTS-PKY1').getByRole('textbox')).toHaveValue('15');
});

test('scene renders in dark theme', async ({ page }) => {
  await page.emulateMedia({ colorScheme: 'light' });
  await openExample(page);
  await expect(page.getByTestId('tire-scene').locator('canvas')).toBeVisible();
  await expect(page.getByTestId('tire-scene-label-x')).toHaveCSS('color', 'rgb(29, 78, 216)');
  await page.getByTestId('theme-toggle').click();
  await expect(page.locator('html')).toHaveClass(/dark/);
  await expect(page.getByTestId('tire-scene').locator('canvas')).toBeVisible();
  await expect(page.getByTestId('tire-scene-label-x')).toHaveCSS('color', 'rgb(96, 165, 250)');
});

test('scene falls back to captions without WebGL', async ({ page }) => {
  await page.addInitScript(() => {
    const original = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (this: HTMLCanvasElement, type: string, ...rest: unknown[]) {
      if (type === 'webgl' || type === 'webgl2') {
        return null;
      }
      return (original as (...args: unknown[]) => unknown).call(this, type, ...rest);
    } as typeof HTMLCanvasElement.prototype.getContext;
  });
  await loadIso(page);
  const fallback = page.getByTestId('tire-scene-fallback');
  await expect(fallback).toBeVisible();
  const items = fallback.getByTestId('tire-scene-captions').locator('li');
  expect(await items.count()).toBeGreaterThan(0);
  const label = await page.getByTestId('tire-scene').getAttribute('aria-label');
  for (const text of await items.allTextContents()) {
    expect(label).toContain(text);
  }
  await expect(page.getByTestId('tire-scene').locator('canvas')).toHaveCount(0);
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
});

test('negative FZ in an ISO model warns', async ({ page }) => {
  await loadIso(page);
  const box = page.getByTestId('step-values-FZ');
  await box.getByTestId('step-add-FZ').fill('-1000');
  await box.getByTestId('step-add-FZ').press('Enter');
  await expect(page.getByTestId('chart-warnings')).toContainText('FZ < 0');
});
