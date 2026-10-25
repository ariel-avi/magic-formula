import type { ColourMode, StepSpec } from './chart-spec';
import type { Combination } from './steps';
import type { Theme } from './theme';

/** Lightness 0..1, chroma >= 0, hue in degrees. */
export interface Oklch {
  l: number;
  c: number;
  h: number;
}

/**
 * Fixed categorical order, never cycled; checked for colour-vision deficiency on adjacent pairs in both themes.
 */
export const SERIES_PALETTES: Readonly<Record<Theme, readonly Oklch[]>> = {
  light: [
    { l: 0.575, c: 0.163, h: 255.5 },
    { l: 0.671, c: 0.175, h: 40.6 },
    { l: 0.669, c: 0.141, h: 162.1 },
    { l: 0.764, c: 0.161, h: 75.1 },
    { l: 0.716, c: 0.141, h: 357.4 },
    { l: 0.529, c: 0.18, h: 142.5 },
    { l: 0.527, c: 0.197, h: 283.4 },
    { l: 0.623, c: 0.191, h: 24.9 },
  ],
  dark: [
    { l: 0.622, c: 0.161, h: 255.1 },
    { l: 0.622, c: 0.173, h: 40.1 },
    { l: 0.621, c: 0.128, h: 163.1 },
    { l: 0.67, c: 0.143, h: 73.2 },
    { l: 0.622, c: 0.171, h: 0.8 },
    { l: 0.529, c: 0.18, h: 142.5 },
    { l: 0.67, c: 0.145, h: 286.8 },
    { l: 0.669, c: 0.159, h: 22.3 },
  ],
};

export const SATURATION_MIN_FACTOR = 0.25;

/** One strategy per colour mode: which colour component a step drives. */
const STRATEGIES: Record<
  ColourMode,
  (colour: Oklch, index: number, count: number, palette: readonly Oklch[]) => Oklch
> = {
  hue: (_colour, index, _count, palette) => palette[index],
  saturation: (colour, index, count) => ({
    ...colour,
    c: colour.c * (count === 1 ? 1 : 1 - (index * (1 - SATURATION_MIN_FACTOR)) / (count - 1)),
  }),
};

/** Modes are applied in this order whatever the step order: hue picks the slot, saturation scales its chroma. */
const ORDER: readonly ColourMode[] = ['hue', 'saturation'];

/** baseSlot: palette slot used when no step drives the hue (the sweep's position in the chart, < MAX_SWEEPS <= 8). */
export function colourFor(combination: Combination, steps: readonly StepSpec[], theme: Theme, baseSlot: number): Oklch {
  const palette = SERIES_PALETTES[theme];
  let colour = palette[baseSlot];
  for (const mode of ORDER) {
    const k = steps.findIndex((step) => step.colourMode === mode);
    if (k >= 0) {
      colour = STRATEGIES[mode](colour, combination.indices[k], steps[k].values.length, palette);
    }
  }
  return colour;
}

/** SVG stroke-dasharray per sweep position ('' = solid); one per allowed sweep, never cycled. */
export const SWEEP_DASHES: readonly string[] = ['', '8 4', '2 3', '12 3 2 3', '4 4', '12 3 2 3 2 3'];

export function toCss(colour: Oklch): string {
  return `oklch(${Number(colour.l.toFixed(3))} ${Number(colour.c.toFixed(3))} ${Number(colour.h.toFixed(1))})`;
}
