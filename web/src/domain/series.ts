import { axisChannel, axisDomain, axisLabel, channelColumn, finiteExtent } from './axes';
import { sweepDisplayName } from './chart';
import type { ChartSpec, StepSpec } from './chart-spec';
import { SWEEP_DASHES, colourFor, toCss } from './colours';
import type { InputChannel, ModelInfo } from './model';
import type { SolvedSweep } from './solve';
import type { Combination } from './steps';
import type { Theme } from './theme';
import { niceExtent, niceTicks } from './ticks';
import { displayUnit, hasUnit } from './units';

export interface SeriesPoint {
  x: number;
  y: number;
}

export interface Series {
  /** `${sweep.id}/s${combination.indices.join('_')}`, unique across the chart. */
  key: string;
  /** comboLabel(...) or, without steps, the Y channel name. Legend items show this. */
  label: string;
  /** `${sweepName} · ${label}` when the chart has more than one sweep, else label. Tooltip and table use this. */
  fullLabel: string;
  colour: string;
  /** SVG stroke-dasharray ('' = solid). */
  dash: string;
  /** One point per sweep grid point, in sweep order (never sorted by x). */
  points: SeriesPoint[];
}

export interface SweepSeries {
  sweepId: string;
  name: string;
  dash: string;
  sweptLabel: string;
  /** batch.grid (display units). */
  grid: number[];
  /** chart.x is this sweep's swept input: the table omits per-series X columns. */
  xIsSwept: boolean;
  series: Series[];
}

export interface ChartData {
  /** Solved sweeps only, chart order. */
  groups: SweepSeries[];
  xLabel: string;
  yLabel: string;
  xDomain: [number, number];
  yDomain: [number, number];
  xTicks: number[];
  yTicks: number[];
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
  chart: ChartSpec,
  solved: readonly SolvedSweep[],
  model: ModelInfo,
  theme: Theme,
): ChartData {
  const xChannel = axisChannel(model, chart.x);
  const yChannel = axisChannel(model, chart.y);
  const multi = chart.sweeps.length > 1;
  const allX: number[] = [];
  const allY: number[] = [];

  const groups = solved.map(({ index, sweep, solution }): SweepSeries => {
    const { batch, outputs } = solution;
    const points = batch.grid.length;
    const xs = channelColumn(sweep, batch, outputs, model, chart.x);
    const ys = channelColumn(sweep, batch, outputs, model, chart.y);
    allX.push(...xs);
    allY.push(...ys);
    const sweptChannel = axisChannel(model, { kind: 'input', name: sweep.swept.variable });
    const name = sweepDisplayName(sweep, index);
    const dash = SWEEP_DASHES[index];
    const series = batch.combinations.map((combination, c): Series => {
      const label = comboLabel(combination, sweep.steps, model.inputs) || chart.y.name;
      return {
        key: `${sweep.id}/s${combination.indices.join('_')}`,
        label,
        fullLabel: multi ? `${name} · ${label}` : label,
        colour: toCss(colourFor(combination, sweep.steps, theme, index)),
        dash,
        points: batch.grid.map((_, i) => ({ x: xs[c * points + i], y: ys[c * points + i] })),
      };
    });
    return {
      sweepId: sweep.id,
      name,
      dash,
      sweptLabel: axisLabel(sweptChannel.name, sweptChannel.unit),
      grid: batch.grid,
      xIsSwept: chart.x.kind === 'input' && chart.x.name === sweep.swept.variable,
      series,
    };
  });

  const xDomain = axisDomain(chart.x.kind, finiteExtent(allX));
  const yDomain = niceExtent(...finiteExtent(allY), 8);
  return {
    groups,
    xLabel: axisLabel(xChannel.name, xChannel.unit),
    yLabel: axisLabel(yChannel.name, yChannel.unit),
    xDomain,
    yDomain,
    xTicks: niceTicks(...xDomain, 8),
    yTicks: niceTicks(...yDomain, 6),
  };
}

export interface TableColumn {
  key: string;
  header: string;
  values: number[];
}

/** The swept grid first, then per series an X column (unless X is the swept input) and a Y column. */
export function tableColumns(group: SweepSeries, xLabel: string, yLabel: string): TableColumn[] {
  const single = group.series.length === 1;
  const columns: TableColumn[] = [{ key: 'swept', header: group.sweptLabel, values: group.grid }];
  for (const s of group.series) {
    if (!group.xIsSwept) {
      columns.push({
        key: `${s.key}/x`,
        header: single ? xLabel : `${s.label} · ${xLabel}`,
        values: s.points.map((p) => p.x),
      });
    }
    columns.push({
      key: `${s.key}/y`,
      header: single ? yLabel : group.xIsSwept ? s.label : `${s.label} · ${yLabel}`,
      values: s.points.map((p) => p.y),
    });
  }
  return columns;
}

/**
 * The sweeps whose every series has one constant X (the X values may differ between series, as with a stepped X input):
 * each curve collapses to a point or a vertical line on the plot.
 */
export function flatXGroups(groups: readonly SweepSeries[]): SweepSeries[] {
  return groups.filter((group) => {
    const finite = group.series.map((s) => s.points.map((p) => p.x).filter(Number.isFinite)).filter((xs) => xs.length > 0);
    return finite.length > 0 && finite.every((xs) => xs.every((x) => x === xs[0]));
  });
}
