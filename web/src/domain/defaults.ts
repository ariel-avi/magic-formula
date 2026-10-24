import type { ChartSpec, SweepSpec } from './chart-spec';
import { findNumber } from './entries';
import type { InputChannel, ModelInfo, TirEntry } from './model';

// Presentation defaults, not model facts: they only pick a sensible first chart.

type Entries = readonly TirEntry[];

function positiveOr(entries: Entries, key: string, fallback: number): number {
  const value = findNumber(entries, key);
  return value !== null && value > 0 ? value : fallback;
}

const FIXED_VALUES: Record<string, (entries: Entries) => number> = {
  FZ: (entries) => positiveOr(entries, 'FNOMIN', 4000),
  VX: (entries) => positiveOr(entries, 'LONGVL', 16.7),
};

export function defaultFixedValue(channel: InputChannel, entries: Entries): number {
  return (FIXED_VALUES[channel.name] ?? (() => 0))(entries);
}

type Range = Omit<SweepSpec, 'variable'>;

const SWEEPS: Record<string, (fz0: number) => Range> = {
  SA: () => ({ start: -15, end: 15, points: 61 }),
  SR: () => ({ start: -0.3, end: 0.3, points: 61 }),
  IA: () => ({ start: -6, end: 6, points: 25 }),
  FZ: (fz0) => ({ start: 0.2 * fz0, end: 2 * fz0, points: 37 }),
  VX: () => ({ start: 1, end: 40, points: 40 }),
};

export function defaultSweep(channel: InputChannel, entries: Entries): SweepSpec {
  const fz0 = FIXED_VALUES.FZ(entries);
  const range = (SWEEPS[channel.name] ?? (() => ({ start: 0, end: 1, points: 11 })))(fz0);
  return { variable: channel.name, ...range };
}

export function defaultChartSpec(model: ModelInfo, entries: Entries, id: string): ChartSpec {
  const fixed = Object.fromEntries(model.inputs.map((channel) => [channel.name, defaultFixedValue(channel, entries)]));
  const find = (name: string) => model.inputs.find((channel) => channel.name === name);
  const sa = find('SA');
  const fz = find('FZ');
  if (sa !== undefined && fz !== undefined && model.outputs.some((channel) => channel.name === 'FY')) {
    const fz0 = fixed.FZ;
    return {
      id,
      sweep: defaultSweep(sa, entries),
      steps: [{ variable: 'FZ', values: [0.5 * fz0, fz0, 1.5 * fz0], colourMode: 'hue' }],
      fixed,
      output: 'FY',
      uncombined: false,
    };
  }
  return {
    id,
    sweep: defaultSweep(model.inputs[0], entries),
    steps: [],
    fixed,
    output: model.outputs[0].name,
    uncombined: false,
  };
}
