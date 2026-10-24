import { describe, expect, it } from 'vitest';
import type { StepSpec } from './chart-spec';
import { colourFor, toCss } from './colours';
import { stepCombinations } from './steps';

const hue = (n: number): StepSpec => ({ variable: 'FZ', values: Array.from({ length: n }, (_, i) => i), colourMode: 'hue' });
const sat = (n: number): StepSpec => ({
  variable: 'IA',
  values: Array.from({ length: n }, (_, i) => i),
  colourMode: 'saturation',
});

const colours = (steps: StepSpec[]) => stepCombinations(steps).map((c) => colourFor(c, steps));

describe('colourFor', () => {
  it('uses the base colour without steps', () => {
    expect(colours([])).toEqual([{ h: 210, s: 90, l: 45 }]);
  });

  it('spreads a hue step around the circle with constant saturation', () => {
    const result = colours([hue(3)]);
    expect(result.map((c) => c.h)).toEqual([210, 330, 90]);
    expect(result.map((c) => c.s)).toEqual([90, 90, 90]);
  });

  it('lowers saturation along a saturation step with constant hue', () => {
    const result = colours([sat(3)]);
    expect(result.map((c) => c.s)).toEqual([90, 57.5, 25]);
    expect(result.map((c) => c.h)).toEqual([210, 210, 210]);
  });

  it('handles a single value in each mode', () => {
    expect(colours([hue(1)])).toEqual([{ h: 210, s: 90, l: 45 }]);
    expect(colours([sat(1)])).toEqual([{ h: 210, s: 90, l: 45 }]);
  });

  it('shares hue and saturation across lines of the same step value', () => {
    const steps = [hue(2), sat(3)];
    const combos = stepCombinations(steps);
    const result = colours(steps);
    combos.forEach((a, i) =>
      combos.forEach((b, j) => {
        if (a.indices[0] === b.indices[0]) {
          expect(result[i].h).toBe(result[j].h);
        }
        if (a.indices[1] === b.indices[1]) {
          expect(result[i].s).toBe(result[j].s);
        }
      }),
    );
    expect(new Set(result.map((c) => c.h)).size).toBe(2);
    expect(new Set(result.map((c) => c.s)).size).toBe(3);
  });
});

describe('toCss', () => {
  it('formats integers', () => {
    expect(toCss({ h: 210, s: 90, l: 45 })).toBe('hsl(210 90% 45%)');
  });

  it('prints fractions with one decimal', () => {
    expect(toCss({ h: 12.34, s: 57.5, l: 45 })).toBe('hsl(12.3 57.5% 45%)');
  });
});
