import type { ChartSpec, StepSpec } from './chart-spec';
import type { SweepBatch } from './batch';
import { colourFor, toCss } from './colours';
import type { InputChannel, OutputChannel } from './model';
import type { Combination } from './steps';
import type { Theme } from './theme';
import { displayUnit, hasUnit } from './units';

export interface Series {
  key: string;
  label: string;
  colour: string;
}

export interface ChartData {
  rows: Array<Record<string, number>>;
  series: Series[];
  xLabel: string;
  yLabel: string;
}

function axisLabel(name: string, siUnit: string): string {
  const { label } = displayUnit(siUnit);
  return hasUnit(label) ? `${name} [${label}]` : name;
}

/** For example "FZ = 4000 N, IA = 2 deg"; empty without steps. */
export function comboLabel(
  combination: Combination,
  steps: readonly StepSpec[],
  inputs: readonly InputChannel[],
): string {
  return steps
    .map((step, k) => {
      const channel = inputs.find((input) => input.name === step.variable)!;
      const { label } = displayUnit(channel.unit);
      return `${step.variable} = ${combination.values[k]}${hasUnit(label) ? ` ${label}` : ''}`;
    })
    .join(', ');
}

export function buildChartData(
  spec: ChartSpec,
  batch: SweepBatch,
  outputs: Float64Array,
  inputs: readonly InputChannel[],
  outputChannels: readonly OutputChannel[],
  theme: Theme,
): ChartData {
  const o = outputChannels.findIndex((channel) => channel.name === spec.output);
  if (o < 0) {
    throw new RangeError(`Unknown output '${spec.output}'`);
  }
  const outputUnit = displayUnit(outputChannels[o].unit);
  const points = spec.sweep.points;

  const series: Series[] = batch.combinations.map((combination) => ({
    key: `s${combination.indices.join('_')}`,
    label: comboLabel(combination, spec.steps, inputs) || spec.output,
    colour: toCss(colourFor(combination, spec.steps, theme)),
  }));

  const rows = batch.grid.map((x, i) => {
    const row: Record<string, number> = { x };
    series.forEach((entry, c) => {
      row[entry.key] = outputUnit.fromSi(outputs[o * batch.rows + c * points + i]);
    });
    return row;
  });

  const sweepChannel = inputs.find((input) => input.name === spec.sweep.variable)!;
  return {
    rows,
    series,
    xLabel: axisLabel(sweepChannel.name, sweepChannel.unit),
    yLabel: axisLabel(outputChannels[o].name, outputChannels[o].unit),
  };
}
