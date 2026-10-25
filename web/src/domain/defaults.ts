import { SWEEP_ID_PREFIX, SWEEP_NAME_PREFIX, type ChartSpec, type SweepSpec, type SweptRange } from './chart-spec';
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

type Range = Omit<SweptRange, 'variable'>;

const SWEEPS: Record<string, (fz0: number) => Range> = {
  SA: () => ({ start: -15, end: 15, points: 61 }),
  SR: () => ({ start: -0.3, end: 0.3, points: 61 }),
  IA: () => ({ start: -6, end: 6, points: 25 }),
  FZ: (fz0) => ({ start: 0.2 * fz0, end: 2 * fz0, points: 37 }),
  VX: () => ({ start: 1, end: 40, points: 40 }),
};

/** Presentation range for sweeping `channel`. */
export function defaultSweptRange(channel: InputChannel, entries: Entries): SweptRange {
  const fz0 = FIXED_VALUES.FZ(entries);
  const range = (SWEEPS[channel.name] ?? (() => ({ start: 0, end: 1, points: 11 })))(fz0);
  return { variable: channel.name, ...range };
}

/** SA swept with an FZ hue step at 0.5/1/1.5 x FZ0 when the model has SA and FZ; otherwise the first input swept without steps. `fixed` holds a default for every input; uncombined is false. */
export function defaultSweepSpec(model: ModelInfo, entries: Entries, id: string, name: string): SweepSpec {
  const fixed = Object.fromEntries(model.inputs.map((channel) => [channel.name, defaultFixedValue(channel, entries)]));
  const find = (channelName: string) => model.inputs.find((channel) => channel.name === channelName);
  const sa = find('SA');
  const fz = find('FZ');
  if (sa !== undefined && fz !== undefined) {
    const fz0 = fixed.FZ;
    return {
      id,
      name,
      swept: defaultSweptRange(sa, entries),
      steps: [{ variable: 'FZ', values: [0.5 * fz0, fz0, 1.5 * fz0], colourMode: 'hue' }],
      fixed,
      uncombined: false,
    };
  }
  return { id, name, swept: defaultSweptRange(model.inputs[0], entries), steps: [], fixed, uncombined: false };
}

/** One sweep `${SWEEP_ID_PREFIX}1` / `${SWEEP_NAME_PREFIX}1`; X = its swept input; Y = FY when the model has it, else outputs[0]. */
export function defaultChartSpec(model: ModelInfo, entries: Entries, id: string): ChartSpec {
  const sweep = defaultSweepSpec(model, entries, `${SWEEP_ID_PREFIX}1`, `${SWEEP_NAME_PREFIX}1`);
  return {
    id,
    sweeps: [sweep],
    x: { kind: 'input', name: sweep.swept.variable },
    y: { kind: 'output', name: model.outputs.some((channel) => channel.name === 'FY') ? 'FY' : model.outputs[0].name },
  };
}
