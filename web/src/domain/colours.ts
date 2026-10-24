import type { ColourMode, StepSpec } from './chart-spec';
import type { Combination } from './steps';

export interface Hsl {
  h: number;
  s: number;
  l: number;
}

export const BASE_HUE = 210;
export const HUE_OFFSET = 210;
export const SATURATION_MAX = 90;
export const SATURATION_MIN = 25;
export const LIGHTNESS = 45;

/** One strategy per colour mode: which colour component a step drives. */
const STRATEGIES: Record<ColourMode, (index: number, count: number) => Partial<Hsl>> = {
  hue: (index, count) => ({ h: (HUE_OFFSET + (index * 360) / count) % 360 }),
  saturation: (index, count) => ({
    s: count === 1 ? SATURATION_MAX : SATURATION_MAX - (index * (SATURATION_MAX - SATURATION_MIN)) / (count - 1),
  }),
};

export function colourFor(combination: Combination, steps: readonly StepSpec[]): Hsl {
  let colour: Hsl = { h: BASE_HUE, s: SATURATION_MAX, l: LIGHTNESS };
  steps.forEach((step, k) => {
    colour = { ...colour, ...STRATEGIES[step.colourMode](combination.indices[k], step.values.length) };
  });
  return colour;
}

function fmt(value: number): string {
  return String(Number(value.toFixed(1)));
}

export function toCss(colour: Hsl): string {
  return `hsl(${fmt(colour.h)} ${fmt(colour.s)}% ${fmt(colour.l)}%)`;
}
