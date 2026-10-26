import { expect, test } from '@playwright/test';
import { curves, firstCurvePath, loadText, openExample, readFixture } from './support';

const PDY1 = 'LATERAL_COEFFICIENTS PDY1';

test('shows the model summary and the parameters next to the chart', async ({ page }) => {
  await openExample(page);
  const ids = ['inputs-pane', 'chart-pane', 'model-pane'];
  for (const id of [...ids, 'model-summary', 'parameter-panel']) {
    await expect(page.getByTestId(id)).toBeVisible();
  }
  const xs: number[] = [];
  for (const id of ids) {
    const box = await page.getByTestId(id).boundingBox();
    xs.push(box?.x ?? Number.NaN);
  }
  expect(xs[0]).toBeLessThan(xs[1]);
  expect(xs[1]).toBeLessThan(xs[2]);
  await expect(page.getByRole('tab', { name: 'Parameters' })).toHaveCount(0);
});

test('prints absent limits as infinity, never as an empty cell', async ({ page }) => {
  await openExample(page);
  const cells = page.getByTestId('model-limits').locator('td:nth-child(2)');
  await expect(cells.first()).toBeVisible();
  for (const text of await cells.allTextContents()) {
    expect(text.trim()).not.toBe('');
  }
});

test('filters parameters by key', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('parameter-filter').fill('PDY');
  const rows = page.locator('[data-testid^="param-row-"]');
  await expect(rows).toHaveCount(3);
  for (const row of await rows.all()) {
    expect(await row.getAttribute('data-testid')).toContain('PDY');
  }
});

test('re-solves the chart after a coefficient edit', async ({ page }) => {
  await openExample(page);
  const before = await firstCurvePath(page);
  const field = page.getByLabel(PDY1, { exact: true });
  await field.fill('1.2');
  await field.press('Enter');
  await expect.poll(() => firstCurvePath(page)).not.toBe(before);
  await expect(page.getByTestId('param-row-LATERAL_COEFFICIENTS-PDY1')).toContainText('edited');
});

test('rejects a non-numeric coefficient without applying it', async ({ page }) => {
  await openExample(page);
  const before = await firstCurvePath(page);
  const field = page.getByLabel(PDY1, { exact: true });
  await field.fill('abc');
  await field.press('Enter');
  const error = page.getByTestId('param-error-LATERAL_COEFFICIENTS-PDY1');
  await expect(error).toHaveText("'abc' is not a finite number.");
  await expect(field).toHaveAttribute('aria-invalid', 'true');
  expect(await firstCurvePath(page)).toBe(before);
  await field.press('Tab');
  await expect(field).toHaveValue('1.0');
  await expect(error).toBeVisible();
  await expect(page.getByTestId('param-row-LATERAL_COEFFICIENTS-PDY1')).not.toContainText('edited');
});

test('lists SI and unsupported units in the [UNITS] dropdown', async ({ page }) => {
  await openExample(page);
  await page.getByRole('combobox', { name: 'UNITS LENGTH' }).click();
  await expect(page.getByRole('option', { name: /^meter/ })).not.toHaveAttribute('aria-disabled', 'true');
  for (const unit of [/^millimeter/, /^inch/]) {
    const option = page.getByRole('option', { name: unit });
    await expect(option).toHaveAttribute('aria-disabled', 'true');
    await expect(option).toContainText('not supported yet');
  }
  await page.keyboard.press('Escape');
});

test('fixes an unsupported unit by choosing the SI unit', async ({ page }) => {
  const text = readFixture('pac2002_S.tir').replace("LENGTH = 'meter'", "LENGTH = 'millimeter'");
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await loadText(page, 'pac2002_S.tir', text);
  await expect(page.getByTestId('load-error')).toBeVisible();
  await page.getByRole('combobox', { name: 'UNITS LENGTH' }).click();
  await page.getByRole('option', { name: /^meter/ }).click();
  await expect(page.getByTestId('load-error')).toBeHidden();
  await expect(curves(page)).toHaveCount(3);
});
