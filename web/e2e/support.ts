import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { expect, type Page } from '@playwright/test';

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
