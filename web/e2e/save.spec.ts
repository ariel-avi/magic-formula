import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';
import { dataPath, openExample } from './support';

test('saves the unedited file byte for byte', async ({ page }) => {
  await openExample(page);
  const [download] = await Promise.all([
    page.waitForEvent('download'),
    page.getByRole('button', { name: 'Save .tir' }).click(),
  ]);
  expect(readFileSync(await download.path()).equals(readFileSync(dataPath('pac2002_S.tir')))).toBe(true);
  expect(download.suggestedFilename()).toBe('pac2002_S.tir');
});

test('saves the edited coefficient', async ({ page }) => {
  await openExample(page);
  const field = page.getByLabel('LATERAL_COEFFICIENTS PDY1', { exact: true });
  await field.fill('1.2');
  await field.press('Enter');
  await expect(page.getByTestId('param-row-LATERAL_COEFFICIENTS-PDY1')).toContainText('edited');
  const [download] = await Promise.all([
    page.waitForEvent('download'),
    page.getByRole('button', { name: 'Save .tir' }).click(),
  ]);
  expect(readFileSync(await download.path(), 'utf8')).toContain('PDY1 = 1.2');
  expect(download.suggestedFilename()).toBe('pac2002_S.tir');
});
