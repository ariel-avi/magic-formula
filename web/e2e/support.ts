import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { expect, type Locator, type Page } from '@playwright/test';

export const DATA_DIR = new URL('../../tests/data/', import.meta.url);
export const dataPath = (name: string): string => fileURLToPath(new URL(name, DATA_DIR));
export const readFixture = (name: string): string => readFileSync(dataPath(name), 'utf8');

/** Opens the app and one of the bundled examples (the button is enabled once the solver is ready). */
export async function openExample(page: Page, fileName = 'pac2002_S.tir'): Promise<void> {
  await page.goto('/');
  await page.getByTestId(`open-example-${fileName}`).click();
  await expect(page.getByTestId('file-name')).toHaveText(fileName);
}

export const curves = (page: Page) => page.getByTestId('sweep-chart').locator('path.recharts-line-curve');

export const strokes = (page: Page): Promise<string[]> =>
  curves(page).evaluateAll((paths) => paths.map((p) => p.getAttribute('stroke') ?? ''));

export const firstCurvePath = async (page: Page): Promise<string> =>
  (await curves(page).first().getAttribute('d')) ?? '';

export const inputRow = (page: Page, name: string) => page.getByTestId(`input-row-${name}`);

export function parseOklch(css: string): { l: number; c: number; h: number } {
  const match = /^oklch\(([\d.]+) ([\d.]+) ([\d.]+)\)$/.exec(css);
  if (match === null) {
    throw new Error(`Not an oklch() colour: ${css}`);
  }
  return { l: Number(match[1]), c: Number(match[2]), h: Number(match[3]) };
}

/** Feeds a file to the hidden file input. */
export async function loadText(page: Page, name: string, text: string | Buffer): Promise<void> {
  await page.getByTestId('file-input').setInputFiles({
    name,
    mimeType: 'text/plain',
    buffer: typeof text === 'string' ? Buffer.from(text, 'utf8') : text,
  });
}

/** Opens the axis Select and picks the option whose accessible name matches. */
export async function selectAxis(page: Page, axis: 'x' | 'y', option: RegExp): Promise<void> {
  await page.getByTestId(`${axis}-axis-select`).click();
  await page.getByRole('option', { name: option }).click();
}

export const dashes = (page: Page): Promise<string[]> =>
  curves(page).evaluateAll((ps) => ps.map((p) => p.getAttribute('stroke-dasharray') ?? ''));

/** Screen points of curve `index`, parsed from its path d attribute (M/L commands), in drawing order. */
export async function curvePoints(page: Page, index: number): Promise<Array<{ x: number; y: number }>> {
  const d = (await curves(page).nth(index).getAttribute('d')) ?? '';
  return [...d.matchAll(/[ML]\s*(-?[\d.]+(?:e-?\d+)?)[ ,](-?[\d.]+(?:e-?\d+)?)/g)].map((m) => ({
    x: Number(m[1]),
    y: Number(m[2]),
  }));
}

/** Elements whose content is wider than their box, described for a readable failure message. */
export const overflowing = (locator: Locator): Promise<string[]> =>
  locator.evaluateAll((els) =>
    els
      .filter((el) => el.scrollWidth > el.clientWidth)
      .map(
        (el) =>
          `${el.getAttribute('aria-label') ?? el.getAttribute('data-testid') ?? el.tagName}: ${(el as HTMLInputElement).value ?? el.textContent} (${el.scrollWidth} > ${el.clientWidth})`,
      ),
  );

/** Fills a field and commits it with Enter. */
export async function commit(locator: Locator, value: string): Promise<void> {
  await locator.fill(value);
  await locator.press('Enter');
}
