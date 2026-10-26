import { describe, expect, it } from 'vitest';
import { MAX_STEP_VALUES, MAX_SWEEPS, type StepSpec } from './chart-spec';
import { SATURATION_MIN_FACTOR, SERIES_PALETTES, SWEEP_DASHES, colourFor, toCss } from './colours';
import { stepCombinations } from './steps';
import type { Theme } from './theme';

const hue = (n: number): StepSpec => ({ variable: 'FZ', values: Array.from({ length: n }, (_, i) => i), colourMode: 'hue' });
const sat = (n: number): StepSpec => ({
  variable: 'IA',
  values: Array.from({ length: n }, (_, i) => i),
  colourMode: 'saturation',
});

const colours = (steps: StepSpec[], theme: Theme) => stepCombinations(steps).map((c) => colourFor(c, steps, theme, 0));

describe('colourFor', () => {
  it('uses the first palette slot without steps', () => {
    expect(colours([], 'light')).toEqual([SERIES_PALETTES.light[0]]);
    expect(colours([], 'dark')).toEqual([SERIES_PALETTES.dark[0]]);
  });

  it('walks the palette slots in order along a hue step', () => {
    expect(colours([hue(3)], 'light')).toEqual(SERIES_PALETTES.light.slice(0, 3));
  });

  it('uses the dark palette in the dark theme', () => {
    expect(colours([hue(3)], 'dark')).toEqual(SERIES_PALETTES.dark.slice(0, 3));
  });

  it('scales chroma along a saturation step at constant lightness and hue', () => {
    const result = colours([sat(3)], 'light');
    const base = SERIES_PALETTES.light[0];
    [1, 0.625, 0.25].forEach((factor, i) => {
      expect(result[i].c).toBeCloseTo(base.c * factor, 12);
      expect(result[i].l).toBe(base.l);
      expect(result[i].h).toBe(base.h);
    });
  });

  it('uses palette[baseSlot] without a hue step', () => {
    for (const theme of ['light', 'dark'] as const) {
      expect(colourFor({ indices: [], values: [] }, [], theme, 3)).toEqual(SERIES_PALETTES[theme][3]);
    }
  });

  it('ignores baseSlot for a hue step', () => {
    const steps = [hue(3)];
    expect(colourFor({ indices: [2], values: [2] }, steps, 'light', 5)).toEqual(SERIES_PALETTES.light[2]);
  });

  it('scales the chroma of palette[baseSlot] for a saturation step', () => {
    const steps = [sat(2)];
    const result = colourFor({ indices: [1], values: [1] }, steps, 'light', 1);
    const base = SERIES_PALETTES.light[1];
    expect(result.c).toBeCloseTo(base.c * SATURATION_MIN_FACTOR, 12);
    expect(result.l).toBe(base.l);
    expect(result.h).toBe(base.h);
  });

  it('keeps full chroma for a single value in each mode', () => {
    expect(colours([hue(1)], 'light')).toEqual([SERIES_PALETTES.light[0]]);
    expect(colours([sat(1)], 'light')).toEqual([SERIES_PALETTES.light[0]]);
  });

  it('composes hue and saturation whatever the step order', () => {
    const factors = [1, 0.625, 0.25];
    for (const steps of [[hue(2), sat(3)], [sat(3), hue(2)]]) {
      const hueStep = steps.findIndex((s) => s.colourMode === 'hue');
      const satStep = steps.findIndex((s) => s.colourMode === 'saturation');
      const combos = stepCombinations(steps);
      const result = colours(steps, 'light');
      combos.forEach((combo, i) => {
        const slot = SERIES_PALETTES.light[combo.indices[hueStep]];
        expect(result[i].l).toBe(slot.l);
        expect(result[i].h).toBe(slot.h);
        expect(result[i].c).toBeCloseTo(slot.c * factors[combo.indices[satStep]], 12);
      });
    }
  });
});

describe('SERIES_PALETTES', () => {
  it('has one slot per allowed step value in each theme', () => {
    expect(SERIES_PALETTES.light).toHaveLength(MAX_STEP_VALUES);
    expect(SERIES_PALETTES.dark).toHaveLength(MAX_STEP_VALUES);
  });
});

describe('SWEEP_DASHES', () => {
  it('has one distinct dash per allowed sweep, the first solid', () => {
    expect(SWEEP_DASHES).toHaveLength(MAX_SWEEPS);
    expect(SWEEP_DASHES[0]).toBe('');
    expect(new Set(SWEEP_DASHES).size).toBe(MAX_SWEEPS);
  });
});

describe('toCss', () => {
  it('formats a palette slot', () => {
    expect(toCss(SERIES_PALETTES.light[0])).toBe('oklch(0.575 0.163 255.5)');
  });

  it('rounds lightness and chroma to three decimals and hue to one', () => {
    expect(toCss({ l: 0.12345, c: 0.0004, h: 12.34 })).toBe('oklch(0.123 0 12.3)');
  });
});
