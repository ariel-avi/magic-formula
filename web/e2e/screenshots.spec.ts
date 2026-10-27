import { readdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { expect, test, type Locator, type Page } from '@playwright/test';
import { commit, curves, dashes, inputRow, openExample, readRepoFile, REPO_DIR, selectAxis } from './support';

// In order, in one worker: the docs check runs after the captures (not serial mode: nothing is ever skipped).
test.describe.configure({ mode: 'default' });

/** Tall enough that the fixed-height app shell shows the whole Tire model pane, warnings included. */
const VIEWPORT_HEIGHT = 1400;
const THEMES = ['light', 'dark'] as const;
type Theme = (typeof THEMES)[number];
/** The renderings of README.md and docs/examples/web.md, in walkthrough order. */
const SHOTS = ['workbench', 'coordinate-system', 'sweeps', 'characteristics', 'table'] as const;
type Shot = (typeof SHOTS)[number];

/** web/test-results/screenshots/ (gitignored) unless MF_SCREENSHOT_DIR is set (resolved against web/). */
const WEB_DIR = new URL('../', import.meta.url);
const SCREENSHOT_DIR: string =
  process.env.MF_SCREENSHOT_DIR !== undefined
    ? resolve(fileURLToPath(WEB_DIR), process.env.MF_SCREENSHOT_DIR)
    : fileURLToPath(new URL('../test-results/screenshots/', import.meta.url));
const DOCS_IMAGE_DIR: string = fileURLToPath(new URL('docs/images/web/', REPO_DIR));
const RENDERING_LINK = /images\/web\/([a-z-]+-(?:light|dark)\.png)/g;

const fileName = (shot: Shot, theme: Theme): string => `${shot}-${theme}.png`;

/** Waits for the web fonts and two animation frames (the lazy three.js scene renders on demand). */
async function settle(page: Page): Promise<void> {
  // No hover tooltip or focus ring from the previous interaction in the picture.
  await page.mouse.move(0, 0);
  await page.evaluate(() => (document.activeElement as HTMLElement | null)?.blur());
  await page.evaluate(() => document.fonts.ready.then(() => undefined));
  await page.evaluate(
    () => new Promise<void>((done) => requestAnimationFrame(() => requestAnimationFrame(() => done()))),
  );
}

/** Settles, then writes `target` (the full page or one pane) to SCREENSHOT_DIR. */
async function capture(page: Page, target: Page | Locator, shot: Shot, theme: Theme): Promise<void> {
  await settle(page);
  const options = {
    path: resolve(SCREENSHOT_DIR, fileName(shot, theme)),
    animations: 'disabled',
    caret: 'hide',
    scale: 'css',
  } as const;
  if (target === page) {
    await page.screenshot({ ...options, fullPage: true });
  } else {
    await (target as Locator).screenshot(options);
  }
}

/** Rendering file names referenced by a Markdown file. */
function referencedRenderings(markdown: string): string[] {
  return [...new Set([...markdown.matchAll(RENDERING_LINK)].map((m) => m[1] as string))].sort();
}

const ALL = SHOTS.flatMap((s) => THEMES.map((t) => fileName(s, t))).sort();

for (const theme of THEMES) {
  test.describe(`${theme} theme`, () => {
    test.use({ colorScheme: theme, viewport: { width: 1600, height: VIEWPORT_HEIGHT }, deviceScaleFactor: 1 });

    test('walks through the workbench and captures every step', async ({ page }) => {
      // Step 1: Open an example
      await page.addInitScript((t) => localStorage.setItem('mf-theme', t), theme);
      await openExample(page);
      await expect(curves(page)).toHaveCount(3);
      await expect(page.getByTestId('coordinate-system-label')).toHaveText('Mixed');
      await expect(page.getByTestId('tire-scene').locator('canvas')).toBeVisible();
      // Nothing in the hero is cut off: the warning card ends inside the viewport.
      const warnings = await page.getByTestId('coordinate-warnings').boundingBox();
      expect(warnings).not.toBeNull();
      expect(warnings!.y + warnings!.height).toBeLessThanOrEqual(VIEWPORT_HEIGHT);
      await capture(page, page, 'workbench', theme);

      // Step 2: Identify and convert the coordinate system
      await page.getByTestId('coordinate-system-ISO').click();
      await expect(page.getByTestId('coordinate-system-label')).toHaveText('ISO');
      const scene = page.getByTestId('tire-scene');
      await expect(scene).toHaveAttribute('data-system', 'ISO');
      await expect(scene.locator('canvas')).toBeVisible();
      await expect(page.getByTestId('coordinate-conversion-note')).toBeVisible();
      await expect(page.locator('[data-testid^="edited-"]').first()).toBeVisible();
      await capture(page, page.getByTestId('coordinate-panel'), 'coordinate-system', theme);

      // Step 3: Compare sweeps
      const chartPane = page.getByTestId('chart-pane');
      await page.getByTestId('sweep-name').fill('Free rolling');
      await page.getByTestId('sweep-duplicate').click();
      await page.getByTestId('sweep-name').fill('Braking, SR = -0.1');
      await commit(page.getByTestId('value-SR'), '-0.1');
      await expect(page.getByTestId('sweep-tab')).toHaveCount(2);
      await expect(curves(page)).toHaveCount(6);
      await expect(page.getByTestId('legend-group')).toHaveCount(2);
      const dash = await dashes(page);
      expect(dash.slice(0, 3)).toEqual(['', '', '']);
      expect(dash[3]).not.toBe('');
      expect(dash.slice(3)).toEqual([dash[3], dash[3], dash[3]]);
      await capture(page, chartPane, 'sweeps', theme);

      // Step 4: Plot a characteristic
      await selectAxis(page, 'y', /^DFY_DSA/);
      await expect(page.getByTestId('sweep-chart')).toContainText('DFY_DSA [N/deg]');
      await expect(curves(page)).toHaveCount(6);
      await capture(page, chartPane, 'characteristics', theme);

      // Step 5: Read the values
      // A coarser sweep, so that both tables fit the pane whole (the last shot, the plots are done).
      for (const tab of [0, 1]) {
        await page.getByTestId('sweep-tab').nth(tab).click();
        await commit(inputRow(page, 'SA').getByTestId('sweep-points'), '13');
      }
      await page.getByRole('tab', { name: 'Table' }).click();
      await expect(page.getByTestId('series-table')).toHaveCount(2);
      await capture(page, chartPane, 'table', theme);
    });
  });
}

test('the docs show exactly the captured renderings', () => {
  expect(referencedRenderings(readRepoFile('docs/examples/web.md'))).toEqual(ALL);
  for (const name of referencedRenderings(readRepoFile('README.md'))) {
    expect(ALL).toContain(name);
  }
  expect(
    readdirSync(DOCS_IMAGE_DIR)
      .filter((n) => n.endsWith('.png'))
      .sort(),
  ).toEqual(ALL);
});
