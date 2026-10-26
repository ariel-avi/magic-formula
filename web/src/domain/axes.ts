import type { SweepBatch } from './batch';
import type { AxisRef, ChannelKind, SweepSpec } from './chart-spec';
import { channelLabel } from './channels';
import type { InputChannel, ModelInfo, OutputChannel } from './model';
import { niceExtent } from './ticks';
import { displayUnit, hasUnit } from './units';

export interface AxisOption {
  /** `${kind}:${name}`; the Select item value (unique even if an input and an output share a name). */
  key: string;
  ref: AxisRef;
  /** "NAME [unit]" in display units ("SA [deg]", "MZ [N·m]"); the name alone for '-' / '' units. */
  label: string;
  /** channelLabel(name).description ('' for unknown channels). */
  description: string;
}

export function axisKey(ref: AxisRef): string {
  return `${ref.kind}:${ref.name}`;
}

export function axisLabel(name: string, siUnit: string): string {
  const { label } = displayUnit(siUnit);
  return hasUnit(label) ? `${name} [${label}]` : name;
}

export function channelsOfKind(model: ModelInfo, kind: ChannelKind): readonly (InputChannel | OutputChannel)[] {
  return kind === 'input' ? model.inputs : model.outputs;
}

/** Inputs first (model order), then outputs (model order). */
export function axisOptions(model: ModelInfo): AxisOption[] {
  const kinds: ChannelKind[] = ['input', 'output'];
  return kinds.flatMap((kind) =>
    channelsOfKind(model, kind).map((channel) => {
      const ref: AxisRef = { kind, name: channel.name };
      return {
        key: axisKey(ref),
        ref,
        label: axisLabel(channel.name, channel.unit),
        description: channelLabel(channel.name).description,
      };
    }),
  );
}

export function axisChannel(model: ModelInfo, ref: AxisRef): InputChannel | OutputChannel {
  const channel = channelsOfKind(model, ref.kind).find((candidate) => candidate.name === ref.name);
  if (channel === undefined) {
    throw new RangeError(`Unknown ${ref.kind} '${ref.name}'`);
  }
  return channel;
}

/** One value per batch row (row r = combination * points + point) of the channel, in display units. */
export function channelColumn(
  sweep: SweepSpec,
  batch: SweepBatch,
  outputs: Float64Array,
  model: ModelInfo,
  ref: AxisRef,
): number[] {
  const channel = axisChannel(model, ref);
  const { rows } = batch;
  const points = batch.grid.length;
  const column: number[] = [];
  if (ref.kind === 'output') {
    const o = model.outputs.findIndex((candidate) => candidate.name === ref.name);
    const unit = displayUnit(channel.unit);
    for (let r = 0; r < rows; r += 1) {
      column.push(unit.fromSi(outputs[o * rows + r]));
    }
    return column;
  }
  const k = sweep.steps.findIndex((step) => step.variable === ref.name);
  for (let r = 0; r < rows; r += 1) {
    if (ref.name === sweep.swept.variable) {
      column.push(batch.grid[r % points]);
    } else if (k >= 0) {
      column.push(batch.combinations[Math.floor(r / points)].values[k]);
    } else {
      column.push(sweep.fixed[ref.name]);
    }
  }
  return column;
}

/** [min, max] of the finite values; [0, 1] when there are none. */
export function finiteExtent(values: readonly number[]): [number, number] {
  let min = Infinity;
  let max = -Infinity;
  for (const value of values) {
    if (Number.isFinite(value)) {
      min = Math.min(min, value);
      max = Math.max(max, value);
    }
  }
  return min > max ? [0, 1] : [min, max];
}

/** An input axis spans its data exactly; an output axis (and a constant input) gets round ends. */
export function axisDomain(kind: ChannelKind, extent: readonly [number, number]): [number, number] {
  return kind === 'input' && extent[0] !== extent[1] ? [extent[0], extent[1]] : niceExtent(extent[0], extent[1], 8);
}
