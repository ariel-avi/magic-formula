import { expect, test } from '@playwright/test';
import { curves, dataPath, loadText, openExample } from './support';

test('shows the empty state with both examples', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('empty-state')).toBeVisible();
  await expect(page.getByRole('button', { name: 'Open example pac2002_S.tir' })).toBeEnabled();
  await expect(page.getByRole('button', { name: 'Open example mf52_S52.tir' })).toBeEnabled();
  await expect(page.getByTestId('empty-state')).toContainText('arctan');
});

test('opens the PAC2002 example and plots three FZ lines of FY against SA', async ({ page }) => {
  await openExample(page);
  await expect(page.getByTestId('model-name')).toContainText('PAC2002');
  const inputs = page.getByTestId('model-inputs');
  for (const name of ['FZ', 'SA', 'SR', 'IA', 'VX']) {
    await expect(inputs.getByTestId(`model-input-${name}`)).toBeVisible();
  }
  await expect(curves(page)).toHaveCount(3);
  const legend = page.getByTestId('legend-item');
  await expect(legend).toHaveCount(3);
  await expect(legend).toHaveText(['FZ = 2000 N', 'FZ = 4000 N', 'FZ = 6000 N']);
});

test('loads a .tir through the file input', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await page.getByTestId('file-input').setInputFiles(dataPath('mf52_S52.tir'));
  await expect(page.getByTestId('file-name')).toHaveText('mf52_S52.tir');
  await expect(page.getByTestId('model-name')).toContainText('FITTYP 6');
  await expect(curves(page)).toHaveCount(3);
});

test('reports a file that is not UTF-8', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await loadText(page, 'bad.tir', Buffer.from([0xff, 0xfe, 0xfd]));
  await expect(page.getByTestId('error-alert')).toContainText('The file is not valid UTF-8 text.');
});

test('switches example from the header menu', async ({ page }) => {
  await openExample(page);
  await page.getByTestId('examples-menu').click();
  await page.getByTestId('example-mf52_S52.tir').click();
  await expect(page.getByTestId('file-name')).toHaveText('mf52_S52.tir');
  await expect(page.getByTestId('model-name')).toContainText('FITTYP 6');
});
