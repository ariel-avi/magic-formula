import { describe, expect, it } from 'vitest';
import { buildBatch } from './batch';
import type { AxisRef, ChartSpec, SweepSpec } from './chart-spec';
import { SERIES_PALETTES, SWEEP_DASHES, colourFor, toCss } from './colours';
import type { InputChannel, ModelInfo } from './model';
import { buildChartData, comboLabel, flatXGroups, tableColumns, type SweepSeries } from './series';
import { solvedSweeps, type SweepOutcome } from './solve';
import { niceExtent, niceTicks } from './ticks';

const channel = (name: string, unit: string): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true });
const inputs = [channel('FZ', 'N'), channel('SA', 'rad'), channel('SR', '-'), channel('IA', 'rad'), channel('VX', 'm/s')];
const model: ModelInfo = {
  name: 'm',
  fittyp: null,
  useMode: null,
  tyreSide: 'LEFT',
  inputs,
  outputs: [
    { name: 'FX', unit: 'N' },
    { name: 'FY', unit: 'N' },
    { name: 'MZ', unit: 'N*m' },
    { name: 'FLAG', unit: '-' },
    { name: 'ANG', unit: 'rad' },
  ],
  limits: [],
};

const sweep = (id: string, overrides: Partial<SweepSpec> = {}): SweepSpec => ({
  id,
  name: '',
  swept: { variable: 'SA', start: -5, end: 5, points: 3 },
  steps: [],
  fixed: { FZ: 4000, SR: 0, IA: 0, VX: 16.7 },
  uncombined: false,
  ...overrides,
});

const twoStepSweep = (id: string): SweepSpec =>
  sweep(id, {
    steps: [
      { variable: 'FZ', values: [3000, 4000], colourMode: 'hue' },
      { variable: 'IA', values: [0, 2, 4], colourMode: 'saturation' },
    ],
    fixed: { SR: 0, VX: 16.7 },
  });

const chartOf = (sweeps: SweepSpec[], x?: AxisRef, y?: AxisRef): ChartSpec => ({
  id: 'c',
  sweeps,
  x: x ?? { kind: 'input', name: 'SA' },
  y: y ?? { kind: 'output', name: 'FY' },
});

function outputsFor(rows: number): Float64Array {
  const data = new Float64Array(rows * model.outputs.length);
  for (let o = 0; o < model.outputs.length; o++) {
    for (let r = 0; r < rows; r++) {
      data[o * rows + r] = o * 1000 + r;
    }
  }
  return data;
}

function solve(chart: ChartSpec, only?: number[]) {
  const outcomes: SweepOutcome[] = chart.sweeps.map((s, i) => {
    if (only !== undefined && !only.includes(i)) {
      return { problems: [], error: 'x', solution: null };
    }
    const batch = buildBatch(s, inputs);
    return { problems: [], error: null, solution: { batch, outputs: outputsFor(batch.rows) } };
  });
  return solvedSweeps(chart.sweeps, outcomes);
}

describe('comboLabel', () => {
  it('lists the step values with units and omits the dash unit', () => {
    const steps = [
      { variable: 'FZ', values: [4000], colourMode: 'hue' as const },
      { variable: 'IA', values: [2], colourMode: 'saturation' as const },
    ];
    expect(comboLabel({ indices: [0, 0], values: [4000, 2] }, steps, inputs)).toBe('FZ = 4000 N, IA = 2 deg');
    const sr = [{ variable: 'SR', values: [0.1], colourMode: 'hue' as const }];
    expect(comboLabel({ indices: [0], values: [0.1] }, sr, inputs)).toBe('SR = 0.1');
  });

  it('omits an empty unit and returns an empty label without steps', () => {
    const bare = [{ ...inputs[0], unit: '' }];
    const step = [{ variable: 'FZ', values: [1], colourMode: 'hue' as const }];
    expect(comboLabel({ indices: [0], values: [1] }, step, bare)).toBe('FZ = 1');
    expect(comboLabel({ indices: [], values: [] }, [], inputs)).toBe('');
  });
});

describe('buildChartData', () => {
  it('builds one group with one series for a single step-less sweep', () => {
    const chart = chartOf([sweep('sweep-1')]);
    const data = buildChartData(chart, solve(chart), model, 'light');
    expect(data.groups).toHaveLength(1);
    const group = data.groups[0];
    expect(group.sweepId).toBe('sweep-1');
    expect(group.name).toBe('Sweep 1');
    expect(group.dash).toBe('');
    expect(group.sweptLabel).toBe('SA [deg]');
    expect(group.grid).toEqual([-5, 0, 5]);
    expect(group.xIsSwept).toBe(true);
    expect(group.series).toEqual([
      {
        key: 'sweep-1/s',
        label: 'FY',
        fullLabel: 'FY',
        colour: toCss(SERIES_PALETTES.light[0]),
        dash: '',
        points: [
          { x: -5, y: 1000 },
          { x: 0, y: 1001 },
          { x: 5, y: 1002 },
        ],
      },
    ]);
    expect(data.xLabel).toBe('SA [deg]');
    expect(data.yLabel).toBe('FY [N]');
    expect(data.xDomain).toEqual([-5, 5]);
    expect(data.xTicks).toEqual(niceTicks(-5, 5, 8));
    expect(data.yDomain).toEqual(niceExtent(1000, 1002, 8));
    expect(data.yTicks).toEqual(niceTicks(...data.yDomain, 6));
  });

  it('uses the dark palette in the dark theme', () => {
    const chart = chartOf([sweep('sweep-1')]);
    const data = buildChartData(chart, solve(chart), model, 'dark');
    expect(data.groups[0].series[0].colour).toBe(toCss(SERIES_PALETTES.dark[0]));
  });

  it('builds one series per combination with unique keys and column-major outputs', () => {
    const chart = chartOf([twoStepSweep('sweep-1')]);
    const data = buildChartData(chart, solve(chart), model, 'light');
    const { series } = data.groups[0];
    expect(series.map((s) => s.key)).toEqual(
      ['s0_0', 's0_1', 's0_2', 's1_0', 's1_1', 's1_2'].map((k) => `sweep-1/${k}`),
    );
    expect(series[4].label).toBe('FZ = 4000 N, IA = 2 deg');
    expect(series[4].points[2].y).toBe(1000 + 4 * 3 + 2);
    const batch = buildBatch(chart.sweeps[0], inputs);
    batch.combinations.forEach((combination, c) =>
      expect(series[c].colour).toBe(toCss(colourFor(combination, chart.sweeps[0].steps, 'light', 0))),
    );
  });

  it('prefixes sweep names, dashes and slots with two sweeps', () => {
    const chart = chartOf([sweep('sweep-1'), sweep('sweep-2')]);
    const data = buildChartData(chart, solve(chart), model, 'light');
    expect(data.groups.map((g) => g.series[0].fullLabel)).toEqual(['Sweep 1 · FY', 'Sweep 2 · FY']);
    expect(data.groups[1].series[0].label).toBe('FY');
    expect(data.groups[1].dash).toBe(SWEEP_DASHES[1]);
    expect(data.groups[1].series[0].dash).toBe(SWEEP_DASHES[1]);
    expect(data.groups[1].series[0].colour).toBe(toCss(SERIES_PALETTES.light[1]));
    const keys = data.groups.flatMap((g) => g.series.map((s) => s.key));
    expect(new Set(keys).size).toBe(2);
  });

  it('keeps the chart position when only the second sweep is solved', () => {
    const chart = chartOf([sweep('sweep-1'), sweep('sweep-2')]);
    const solved = solve(chart, [1]);
    expect(solved).toHaveLength(1);
    expect(solved[0].index).toBe(1);
    const data = buildChartData(chart, solved, model, 'light');
    expect(data.groups).toHaveLength(1);
    expect(data.groups[0].series[0].fullLabel).toBe('Sweep 2 · FY');
    expect(data.groups[0].dash).toBe(SWEEP_DASHES[1]);
  });

  it('plots an output on X from its column', () => {
    const chart = chartOf([sweep('sweep-1')], { kind: 'output', name: 'FX' });
    const data = buildChartData(chart, solve(chart), model, 'light');
    expect(data.groups[0].xIsSwept).toBe(false);
    expect(data.groups[0].series[0].points.map((p) => p.x)).toEqual([0, 1, 2]);
    expect(data.xDomain).toEqual(niceExtent(0, 2, 8));
    expect(data.xLabel).toBe('FX [N]');
  });

  it('plots a stepped input on X as a constant per series', () => {
    const chart = chartOf([twoStepSweep('sweep-1')], { kind: 'input', name: 'FZ' });
    const data = buildChartData(chart, solve(chart), model, 'light');
    const { series } = data.groups[0];
    expect(series[0].points.map((p) => p.x)).toEqual([3000, 3000, 3000]);
    expect(series[3].points.map((p) => p.x)).toEqual([4000, 4000, 4000]);
    expect(data.groups[0].xIsSwept).toBe(false);
  });

  it('widens the domain of a constant fixed input on X', () => {
    const chart = chartOf([sweep('sweep-1')], { kind: 'input', name: 'SR' });
    expect(buildChartData(chart, solve(chart), model, 'light').xDomain).toEqual([-1, 1]);
  });

  it('plots an input on Y', () => {
    const chart = chartOf([sweep('sweep-1')], undefined, { kind: 'input', name: 'SA' });
    const data = buildChartData(chart, solve(chart), model, 'light');
    expect(data.groups[0].series[0].points.map((p) => p.y)).toEqual([-5, 0, 5]);
    expect(data.groups[0].series[0].label).toBe('SA');
  });

  it('labels moment and flag axes', () => {
    const mz = chartOf([sweep('sweep-1')], undefined, { kind: 'output', name: 'MZ' });
    expect(buildChartData(mz, solve(mz), model, 'light').yLabel).toBe('MZ [N·m]');
    const flag = chartOf([sweep('sweep-1')], undefined, { kind: 'output', name: 'FLAG' });
    expect(buildChartData(flag, solve(flag), model, 'light').yLabel).toBe('FLAG');
  });

  it('converts angle outputs to degrees', () => {
    const chart = chartOf([sweep('sweep-1')], undefined, { kind: 'output', name: 'ANG' });
    const solved = solve(chart);
    solved[0].solution.outputs[4 * 3] = Math.PI;
    const data = buildChartData(chart, solved, model, 'light');
    expect(data.groups[0].series[0].points[0].y).toBeCloseTo(180, 10);
  });

  it('throws RangeError for an unknown axis, even without solved sweeps', () => {
    const badX = chartOf([sweep('sweep-1')], { kind: 'output', name: 'ZZ' });
    expect(() => buildChartData(badX, solve(badX), model, 'light')).toThrow(RangeError);
    const badY = chartOf([sweep('sweep-1')], undefined, { kind: 'output', name: 'ZZ' });
    expect(() => buildChartData(badY, [], model, 'light')).toThrow(RangeError);
  });

  it('gives default domains without solved sweeps', () => {
    const data = buildChartData(chartOf([sweep('sweep-1')]), [], model, 'light');
    expect(data.groups).toEqual([]);
    expect(data.xDomain).toEqual([0, 1]);
  });
});

describe('tableColumns', () => {
  const series = (key: string, label: string, xs: number[], ys: number[]) => ({
    key,
    label,
    fullLabel: label,
    colour: '',
    dash: '',
    points: xs.map((x, i) => ({ x, y: ys[i] })),
  });
  const group = (xIsSwept: boolean, count: number): SweepSeries => ({
    sweepId: 'sweep-1',
    name: 'Sweep 1',
    dash: '',
    sweptLabel: 'SA [deg]',
    grid: [1, 2],
    xIsSwept,
    series: Array.from({ length: count }, (_, i) => series(`k${i}`, `L${i}`, [10 + i, 20 + i], [30 + i, 40 + i])),
  });

  it('uses the Y label for one series on the swept X', () => {
    const columns = tableColumns(group(true, 1), 'SA [deg]', 'FY [N]');
    expect(columns.map((c) => c.header)).toEqual(['SA [deg]', 'FY [N]']);
    expect(columns.map((c) => c.key)).toEqual(['swept', 'k0/y']);
    expect(columns[0].values).toEqual([1, 2]);
    expect(columns[1].values).toEqual([30, 40]);
  });

  it('uses the series labels for several series on the swept X', () => {
    const columns = tableColumns(group(true, 2), 'SA [deg]', 'FY [N]');
    expect(columns.map((c) => c.header)).toEqual(['SA [deg]', 'L0', 'L1']);
  });

  it('adds an X column for one series on another X', () => {
    const columns = tableColumns(group(false, 1), 'FX [N]', 'FY [N]');
    expect(columns.map((c) => c.header)).toEqual(['SA [deg]', 'FX [N]', 'FY [N]']);
    expect(columns.map((c) => c.key)).toEqual(['swept', 'k0/x', 'k0/y']);
    expect(columns[1].values).toEqual([10, 20]);
    expect(columns[2].values).toEqual([30, 40]);
  });

  it('qualifies both columns for several series on another X', () => {
    const columns = tableColumns(group(false, 2), 'FX [N]', 'FY [N]');
    expect(columns.map((c) => c.header)).toEqual([
      'SA [deg]',
      'L0 · FX [N]',
      'L0 · FY [N]',
      'L1 · FX [N]',
      'L1 · FY [N]',
    ]);
  });
});

describe('flatXGroups', () => {
  const group = (sweepId: string, xs: number[]): SweepSeries => ({
    sweepId,
    name: sweepId,
    dash: '',
    sweptLabel: 'SA [deg]',
    grid: xs,
    xIsSwept: false,
    series: [{ key: 'k', label: 'L', fullLabel: 'L', colour: '', dash: '', points: xs.map((x) => ({ x, y: 0 })) }],
  });

  it('keeps only the sweeps with a constant X', () => {
    const flat = group('flat', [3, 3, 3]);
    expect(flatXGroups([group('varying', [1, 2, 3]), flat])).toEqual([flat]);
  });

  it('flags a sweep whose series each have their own constant X', () => {
    const stepped: SweepSeries = {
      ...group('stepped', [1, 1]),
      series: [
        { key: 'a', label: 'A', fullLabel: 'A', colour: '', dash: '', points: [{ x: 1, y: 0 }, { x: 1, y: 1 }] },
        { key: 'b', label: 'B', fullLabel: 'B', colour: '', dash: '', points: [{ x: 2, y: 0 }, { x: 2, y: 1 }] },
      ],
    };
    expect(flatXGroups([stepped])).toEqual([stepped]);
  });

  it('does not flag a sweep when any series varies', () => {
    const mixed: SweepSeries = {
      ...group('mixed', [1, 1]),
      series: [
        { key: 'a', label: 'A', fullLabel: 'A', colour: '', dash: '', points: [{ x: 1, y: 0 }, { x: 1, y: 1 }] },
        { key: 'b', label: 'B', fullLabel: 'B', colour: '', dash: '', points: [{ x: 1, y: 0 }, { x: 2, y: 1 }] },
      ],
    };
    expect(flatXGroups([mixed])).toEqual([]);
  });

  it('ignores non-finite values and sweeps without any finite X', () => {
    const flat = group('flat', [3, NaN, 3]);
    expect(flatXGroups([flat, group('none', [NaN, NaN])])).toEqual([flat]);
  });
});
