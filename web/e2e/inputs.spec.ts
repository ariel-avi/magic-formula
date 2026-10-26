import { expect, test } from '@playwright/test';
import { MAX_STEPS } from '../src/domain/chart-spec';
import { curves, firstCurvePath, inputRow, openExample } from './support';

test('lists every model input with its role', async ({ page }) => {
  await openExample(page);
  const roles: Record<string, string> = { FZ: 'step', SA: 'sweep', SR: 'value', IA: 'value', VX: 'value' };
  for (const [name, role] of Object.entries(roles)) {
    await expect(inputRow(page, name)).toHaveAttribute('data-role', role);
  }
});

test('moves the sweep to another input', async ({ page }) => {
  await openExample(page);
  await inputRow(page, 'SR').getByRole('radio', { name: 'Sweep' }).click();
  await expect(inputRow(page, 'SR')).toHaveAttribute('data-role', 'sweep');
  await expect(inputRow(page, 'SA')).toHaveAttribute('data-role', 'value');
  await expect(inputRow(page, 'SR').getByTestId('sweep-start')).toHaveValue('-0.3');
  await expect(inputRow(page, 'SA').getByTestId('value-SA')).toBeVisible();
});

test('locks Step and Value on the swept input', async ({ page }) => {
  await openExample(page);
  const row = inputRow(page, 'SA');
  await expect(row.getByRole('radio', { name: 'Step' })).toBeDisabled();
  await expect(row.getByRole('radio', { name: 'Value' })).toBeDisabled();
  await page.getByTestId('role-option-SA-step').hover({ force: true });
  await expect(page.getByRole('tooltip')).toContainText('One input is always swept');
});

test('allows at most two stepped inputs', async ({ page }) => {
  await openExample(page);
  await inputRow(page, 'IA').getByRole('radio', { name: 'Step' }).click();
  await expect(inputRow(page, 'IA')).toHaveAttribute('data-role', 'step');
  await expect(inputRow(page, 'VX').getByRole('radio', { name: 'Step' })).toBeDisabled();
  await page.getByTestId('role-option-VX-step').hover({ force: true });
  await expect(page.getByRole('tooltip')).toContainText(`At most ${MAX_STEPS} inputs can be stepped.`);
});

test('adds and removes step values as chips', async ({ page }) => {
  await openExample(page);
  const box = page.getByTestId('step-values-FZ');
  const chips = box.getByTestId('step-chip');
  await box.getByTestId('step-add-FZ').fill('8000');
  await box.getByTestId('step-add-FZ').press('Enter');
  await expect(chips).toHaveCount(4);
  await expect(curves(page)).toHaveCount(4);
  await box.getByTestId('step-add-FZ').fill('3000');
  await box.getByRole('button', { name: 'Add value' }).click();
  await expect(chips).toHaveCount(5);
  expect(await chips.evaluateAll((els) => els.map((e) => e.getAttribute('data-value')))).toEqual([
    '2000',
    '3000',
    '4000',
    '6000',
    '8000',
  ]);
  await box.getByRole('button', { name: 'Remove 8000' }).click();
  await box.getByRole('button', { name: 'Remove 3000' }).click();
  await expect(chips).toHaveCount(3);
  await expect(curves(page)).toHaveCount(3);
  await box.getByRole('button', { name: 'Remove 2000' }).click();
  await box.getByRole('button', { name: 'Remove 4000' }).click();
  await expect(chips).toHaveCount(1);
  await expect(box.getByRole('button', { name: 'Remove 6000' })).toBeDisabled();
});

test('rejects an invalid or duplicate step value', async ({ page }) => {
  await openExample(page);
  const box = page.getByTestId('step-values-FZ');
  const add = box.getByTestId('step-add-FZ');
  await add.fill('abc');
  await add.press('Enter');
  await expect(page.getByTestId('step-error-FZ')).toHaveText("'abc' is not a finite number.");
  await expect(box.getByTestId('step-chip')).toHaveCount(3);
  await expect(add).toHaveValue('abc');
  await add.fill('4000');
  await add.press('Enter');
  await expect(page.getByTestId('step-error-FZ')).toHaveText('4000 is already in the list.');
});

test('fills step values from a range', async ({ page }) => {
  await openExample(page);
  const box = page.getByTestId('step-values-FZ');
  await box.getByRole('button', { name: 'Range…' }).click();
  await page.getByLabel('FZ range from', { exact: true }).fill('1000');
  await page.getByLabel('FZ range to', { exact: true }).fill('5000');
  await page.getByLabel('FZ range count', { exact: true }).fill('5');
  await page.getByRole('button', { name: 'Fill' }).click();
  const chips = box.getByTestId('step-chip');
  await expect(chips).toHaveCount(5);
  expect(await chips.evaluateAll((els) => els.map((e) => e.getAttribute('data-value')))).toEqual([
    '1000',
    '2000',
    '3000',
    '4000',
    '5000',
  ]);
  await expect(curves(page)).toHaveCount(5);
});

test('changes a held value and re-solves', async ({ page }) => {
  await openExample(page);
  const before = await firstCurvePath(page);
  await page.getByTestId('value-SR').fill('0.1');
  await expect.poll(() => firstCurvePath(page)).not.toBe(before);
});
