import { expect, test, type Page } from '@playwright/test';
import { columnTexts, curvePoints, curves, loadText, openExample, readFixture, selectAxis } from './support';

const fmt = (x: number): string => String(Number(x.toPrecision(6)));
const RAD_TO_DEG = 180 / Math.PI;
const PEAK_SA = 0.2475646539106;

async function openTable(page: Page): Promise<void> {
  await page.getByRole('tab', { name: 'Table' }).click();
  await expect(page.getByTestId('series-table').first()).toBeVisible();
}

async function loadModel(page: Page, name: string, text: string): Promise<void> {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await loadText(page, name, text);
  await expect(page.getByTestId('file-name')).toHaveText(name);
}

test('lists the eight characteristics in their own axis group', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('y-axis-select').click();
  await expect(
    page.locator('[data-slot="select-label"]').getByText('Characteristics', { exact: true }),
  ).toBeVisible();
  const patterns = [
    /^DFY_DSA \[N\/deg\]/,
    /^DFX_DSR/,
    /^FY_OVER_SA \[N\/deg\]/,
    /^FX_OVER_SR/,
    /^DY/,
    /^DX/,
    /^SA_AT_PEAK_FY \[deg\]/,
    /^SR_AT_PEAK_FX/,
  ];
  for (const pattern of patterns) {
    await expect(page.getByRole('option', { name: pattern })).toHaveCount(1);
  }
});

test('plots the instantaneous cornering stiffness in N/deg', async ({ page }) => {
  await openExample(page);
  await selectAxis(page, 'y', /^DFY_DSA/);
  await expect(curves(page)).toHaveCount(3);
  await expect(page.getByTestId('sweep-chart')).toContainText('DFY_DSA [N/deg]');
  await openTable(page);
  const cells = await columnTexts(page, 'FZ = 4000 N');
  expect(cells[30]).toBe(fmt(-55384.61537396 / RAD_TO_DEG));
});

test('shows the slip angle at peak lateral force in degrees', async ({ page }) => {
  await openExample(page);
  await selectAxis(page, 'y', /^SA_AT_PEAK_FY/);
  await expect(curves(page)).toHaveCount(3);
  await openTable(page);
  const cells = await columnTexts(page, 'FZ = 4000 N');
  expect(cells[20]).toBe(fmt(-PEAK_SA * RAD_TO_DEG));
  expect(cells[40]).toBe(fmt(PEAK_SA * RAD_TO_DEG));
});

test('draws a gap and a dash where a peak slip is undefined', async ({ page }) => {
  const errors: string[] = [];
  page.on('console', (message) => {
    if (message.type() === 'error') errors.push(message.text());
  });
  await loadModel(page, 'pac2002_S_rng.tir', readFixture('pac2002_S_rng.tir').replace('ALPMAX = 1.5', 'ALPMAX = 0.1'));
  await selectAxis(page, 'y', /^SA_AT_PEAK_FY/);
  await expect(curves(page)).toHaveCount(3);
  for (let i = 0; i < 3; i += 1) {
    expect(await curvePoints(page, i)).toHaveLength(30);
  }
  await openTable(page);
  for (const header of ['FZ = 2000 N', 'FZ = 4000 N', 'FZ = 6000 N']) {
    const cells = await columnTexts(page, header);
    expect(cells.slice(30, 61)).toEqual(Array<string>(31).fill('—'));
    expect(cells).not.toContain('0');
  }
  expect(errors).toEqual([]);
});

test('explains an axis with no value at any point', async ({ page }) => {
  await loadModel(page, 'pac2002_S.tir', readFixture('pac2002_S.tir').replace('PCY1 = 1.3', 'PCY1 = 1.0'));
  await selectAxis(page, 'y', /^SA_AT_PEAK_FY/);
  await expect(page.getByTestId('undefined-axes')).toBeVisible();
  await expect(page.getByTestId('undefined-axes')).toContainText('SA_AT_PEAK_FY [deg]');
  await openTable(page);
  const cells = await columnTexts(page, 'FZ = 4000 N');
  expect(cells).toHaveLength(61);
  expect(cells.every((c) => c === '—')).toBe(true);
});

test('shows the characteristics of the converted model after the ISO and SAE toggles', async ({ page }) => {
  await loadModel(page, 'pac2002_S.tir', readFixture('pac2002_S.tir'));
  await selectAxis(page, 'y', /^DFY_DSA/);
  await openTable(page);
  const mixed = (await columnTexts(page, 'FZ = 4000 N'))[30];
  expect(mixed.startsWith('-')).toBe(true);

  await page.getByTestId('coordinate-system-ISO').click();
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
  await expect.poll(async () => (await columnTexts(page, 'FZ = 4000 N'))[30]).toBe(mixed.slice(1));

  await page.getByRole('tab', { name: 'Plot' }).click();
  await selectAxis(page, 'y', /^DY/);
  await openTable(page);
  expect((await columnTexts(page, 'FZ = 4000 N'))[30]).toBe('4000');

  await page.getByTestId('coordinate-system-SAE').click();
  await expect(page.getByTestId('coordinate-system-label')).toHaveText('SAE');
  await expect.poll(async () => (await columnTexts(page, 'FZ = -4000 N'))[30]).toBe('-4000');
});

test('lists the characteristics in the model summary', async ({ page }) => {
  await openExample(page);
  const summary = page.getByTestId('model-characteristics');
  await expect(summary.locator('[data-testid^="model-characteristic-"]')).toHaveCount(8);
  await expect(page.getByTestId('model-characteristic-DFY_DSA')).toContainText('N/deg');
});
