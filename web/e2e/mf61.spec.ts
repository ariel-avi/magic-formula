import { expect, test } from '@playwright/test';
import { curves, dataPath, inputRow, selectAxis } from './support';

test('MF6.1 exposes pressure, turn slip, radii and characteristics', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('open-example-pac2002_S.tir')).toBeEnabled();
  await page.getByTestId('file-input').setInputFiles(dataPath('mf61_solver.tir'));
  await expect(page.getByTestId('model-name')).toContainText('MF 6.1');
  await expect(inputRow(page, 'P')).toBeVisible();
  await expect(inputRow(page, 'TS')).toBeVisible();
  const turnSlip = page.getByTestId('turn-slip-switch');
  await expect(turnSlip).not.toBeChecked();
  await expect(curves(page)).toHaveCount(3);
  await turnSlip.click();
  await expect(turnSlip).toBeChecked();
  await expect(curves(page)).toHaveCount(3);
  await selectAxis(page, 'y', /^RE \[m\]$/);
  await expect(page.getByTestId('y-axis-select')).toHaveText('RE [m]');
  await expect(page.getByTestId('sweep-chart')).toContainText('RE [m]');
  await expect(curves(page)).toHaveCount(3);
  await selectAxis(page, 'y', /^DFY_DSA \[N\/deg\]$/);
  await expect(page.getByTestId('y-axis-select')).toHaveText('DFY_DSA [N/deg]');
  await expect(page.getByTestId('sweep-chart')).toContainText('DFY_DSA [N/deg]');
  await expect(curves(page)).toHaveCount(3);
});
