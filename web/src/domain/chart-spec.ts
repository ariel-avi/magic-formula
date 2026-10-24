export type ColourMode = 'hue' | 'saturation';

/** Values are in display units (see units.ts). */
export interface SweepSpec {
  variable: string;
  start: number;
  end: number;
  points: number;
}

export interface StepSpec {
  variable: string;
  values: number[];
  colourMode: ColourMode;
}

export interface ChartSpec {
  id: string;
  sweep: SweepSpec;
  steps: StepSpec[];
  fixed: Record<string, number>;
  output: string;
  uncombined: boolean;
}

export const MIN_SWEEP_POINTS = 2;
export const MAX_SWEEP_POINTS = 501;
export const MAX_STEPS = 2;
export const MAX_STEP_VALUES = 8; // one value per categorical palette slot
