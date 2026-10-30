export type ColourMode = 'hue' | 'saturation';

/** The swept input and its range; values are in display units (see units.ts). */
export interface SweptRange {
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

/** One self-contained sweep: one swept input, 0..MAX_STEPS stepped inputs, a held value for every other input, and the slip mode. */
export interface SweepSpec {
  /** Unique within the chart ('sweep-1', 'sweep-2', ...); React key and tab value. */
  id: string;
  /** User-visible name; a blank name is shown as "Sweep n" (see sweepDisplayName). */
  name: string;
  swept: SweptRange;
  steps: StepSpec[];
  /** Held values in display units, keyed by input name. */
  fixed: Record<string, number>;
  /** Pure slip: the solver evaluates without combined-slip coupling. */
  uncombined: boolean;
  /** Enable MF6.1 turn-slip corrections; absent means false. */
  turnSlip?: boolean;
}

export type ChannelKind = 'input' | 'output' | 'characteristic';

/** A model channel used as a chart axis. Names may coincide across kinds, hence the kind. */
export interface AxisRef {
  kind: ChannelKind;
  name: string;
}

export interface ChartSpec {
  id: string;
  /** 1..MAX_SWEEPS sweeps, drawn and listed in this order. */
  sweeps: SweepSpec[];
  x: AxisRef;
  y: AxisRef;
}

export const MIN_SWEEP_POINTS = 2;
export const MAX_SWEEP_POINTS = 501;
export const MAX_STEPS = 2;
export const MAX_STEP_VALUES = 8; // one value per categorical palette slot
export const MAX_SWEEPS = 6; // one dash pattern per sweep (colours.ts SWEEP_DASHES)
export const MAX_SWEEP_NAME_LENGTH = 24;
export const SWEEP_ID_PREFIX = 'sweep-';
export const SWEEP_NAME_PREFIX = 'Sweep ';
