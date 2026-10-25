import { expect, test } from '@playwright/test';
import { commit, openExample, overflowing } from './support';

const LONG = '-1234.56789012';

for (const viewport of [
  { width: 390, height: 844 },
  { width: 1024, height: 800 },
  { width: 1600, height: 1000 },
]) {
  test.describe(`viewport ${viewport.width}x${viewport.height}`, () => {
    test.use({ viewport });

    test('shows a long sweep name without clipping', async ({ page }) => {
      await openExample(page);
      const name = page.getByTestId('sweep-name');
      await name.fill('PURE LATERAL HIGH LOAD 2');
      expect(await overflowing(name)).toEqual([]);
    });

    test('shows every value without clipping', async ({ page }) => {
      await openExample(page);
      await commit(page.getByTestId('sweep-start'), LONG);
      await commit(page.getByTestId('sweep-end'), '1234.56789012');
      for (const name of ['SR', 'IA', 'VX']) {
        await commit(page.getByTestId(`value-${name}`), LONG);
      }

      const add = page.getByTestId('step-add-FZ');
      await add.fill(LONG);
      expect(await overflowing(add)).toEqual([]);
      await add.press('Enter');

      expect(
        await overflowing(
          page
            .getByTestId('inputs-pane')
            .locator(
              'input:not([type="checkbox"]):not([type="hidden"]):not([aria-hidden="true"]), [data-testid="step-chip"]',
            ),
        ),
      ).toEqual([]);

      await page.getByTestId('step-values-FZ').getByRole('button', { name: 'Range…' }).click();
      await page.getByLabel('FZ range from', { exact: true }).fill(LONG);
      await page.getByLabel('FZ range to', { exact: true }).fill(LONG);
      await page.getByLabel('FZ range count', { exact: true }).fill('8');
      expect(await overflowing(page.locator('[data-slot="popover-content"] input'))).toEqual([]);
      await page.keyboard.press('Escape');

      await expect(page.getByTestId('param-row-MODEL-PROPERTY_FILE_FORMAT')).toBeVisible();
      await commit(page.getByLabel('MODEL LONGVL', { exact: true }), LONG);
      expect(
        await overflowing(page.locator('[data-testid^="param-row-"] td, [data-testid="parameter-panel"] input')),
      ).toEqual([]);
    });
  });
}
