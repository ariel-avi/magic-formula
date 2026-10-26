import { channelsOfKind } from './axes';
import {
  MAX_SWEEPS,
  MAX_SWEEP_NAME_LENGTH,
  SWEEP_ID_PREFIX,
  SWEEP_NAME_PREFIX,
  type ChartSpec,
  type SweepSpec,
} from './chart-spec';
import { defaultSweepSpec } from './defaults';
import type { ModelInfo, TirEntry } from './model';

/** The trimmed name, or "Sweep n" (n = index + 1) when blank. */
export function sweepDisplayName(sweep: SweepSpec, index: number): string {
  return sweep.name.trim() || `${SWEEP_NAME_PREFIX}${index + 1}`;
}

export function sweepIndexOf(sweeps: readonly SweepSpec[], id: string): number {
  return Math.max(0, sweeps.findIndex((sweep) => sweep.id === id));
}

function firstFree(prefix: string, taken: ReadonlySet<string>): string {
  for (let n = 1; ; n += 1) {
    if (!taken.has(`${prefix}${n}`)) {
      return `${prefix}${n}`;
    }
  }
}

function freshIdentity(chart: ChartSpec): { id: string; name: string } {
  return {
    id: firstFree(SWEEP_ID_PREFIX, new Set(chart.sweeps.map((sweep) => sweep.id))),
    name: firstFree(SWEEP_NAME_PREFIX, new Set(chart.sweeps.map((sweep, i) => sweepDisplayName(sweep, i)))),
  };
}

/** Appends the default sweep; ignored at MAX_SWEEPS. */
export function addSweep(chart: ChartSpec, model: ModelInfo, entries: readonly TirEntry[]): ChartSpec {
  if (chart.sweeps.length >= MAX_SWEEPS) {
    return chart;
  }
  const { id, name } = freshIdentity(chart);
  return {
    ...chart,
    sweeps: [...chart.sweeps, defaultSweepSpec(model, entries, id, name)],
  };
}

/** Inserts a deep copy right after `index`; ignored at MAX_SWEEPS. */
export function duplicateSweep(chart: ChartSpec, index: number): ChartSpec {
  if (chart.sweeps.length >= MAX_SWEEPS) {
    return chart;
  }
  const copy: SweepSpec = { ...structuredClone(chart.sweeps[index]), ...freshIdentity(chart) };
  return {
    ...chart,
    sweeps: [...chart.sweeps.slice(0, index + 1), copy, ...chart.sweeps.slice(index + 1)],
  };
}

/** Removes the sweep; the last remaining sweep cannot be removed. */
export function removeSweep(chart: ChartSpec, index: number): ChartSpec {
  if (chart.sweeps.length <= 1) {
    return chart;
  }
  return { ...chart, sweeps: chart.sweeps.filter((_, i) => i !== index) };
}

/** Stores the name cut to MAX_SWEEP_NAME_LENGTH, untrimmed so typing a space works. */
export function renameSweep(chart: ChartSpec, index: number, name: string): ChartSpec {
  return {
    ...chart,
    sweeps: chart.sweeps.map((sweep, i) => (i === index ? { ...sweep, name: name.slice(0, MAX_SWEEP_NAME_LENGTH) } : sweep)),
  };
}

/** Replaces a blank name with the first free "Sweep n", so the displayed name no longer depends on the position. */
export function freezeBlankName(chart: ChartSpec, index: number): ChartSpec {
  if (chart.sweeps[index].name.trim() !== '') {
    return chart;
  }
  const others = new Set(chart.sweeps.flatMap((sweep, i) => (i === index ? [] : [sweepDisplayName(sweep, i)])));
  return renameSweep(chart, index, firstFree(SWEEP_NAME_PREFIX, others));
}

/** Replaces one sweep; the X axis follows when it showed the old swept input that no sweep sweeps any more. */
export function replaceSweep(chart: ChartSpec, index: number, next: SweepSpec): ChartSpec {
  if (next === chart.sweeps[index]) {
    return chart;
  }
  const before = chart.sweeps[index].swept.variable;
  const sweeps = chart.sweeps.map((sweep, i) => (i === index ? next : sweep));
  const follow = chart.x.kind === 'input' && chart.x.name === before && !sweeps.some((sweep) => sweep.swept.variable === before);
  return { ...chart, sweeps, x: follow ? { kind: 'input', name: next.swept.variable } : chart.x };
}

/** One message per axis that names a channel the model lacks. */
export function validateAxes(chart: ChartSpec, model: ModelInfo): string[] {
  const axes = [
    ['X', chart.x],
    ['Y', chart.y],
  ] as const;
  return axes
    .filter(([, ref]) => !channelsOfKind(model, ref.kind).some((channel) => channel.name === ref.name))
    .map(([label, ref]) => `Unknown ${label} axis ${ref.kind} '${ref.name}'.`);
}

const names = (channels: readonly { name: string }[]) => channels.map((channel) => channel.name);
const sameNames = (a: readonly string[], b: readonly string[]) =>
  a.length === b.length && a.every((name, index) => name === b[index]);

/** True when the input or output channel names differ (a missing previous model counts as changed). */
export function channelsChanged(previous: ModelInfo | null, next: ModelInfo): boolean {
  return (
    previous === null ||
    !sameNames(names(previous.inputs), names(next.inputs)) ||
    !sameNames(names(previous.outputs), names(next.outputs))
  );
}

/** True when a swept variable, a step variable or an axis channel is absent from the model. */
export function referencesMissing(chart: ChartSpec, model: ModelInfo): boolean {
  const inputs = names(model.inputs);
  return (
    chart.sweeps.some(
      (sweep) => !inputs.includes(sweep.swept.variable) || sweep.steps.some((step) => !inputs.includes(step.variable)),
    ) || validateAxes(chart, model).length > 0
  );
}
