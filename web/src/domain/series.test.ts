import { describe, expect, it } from 'vitest';
import { buildBatch } from './batch';
import type { ChartSpec } from './chart-spec';
import { SERIES_PALETTES, colourFor, toCss } from './colours';
import type { InputChannel, OutputChannel } from './model';
import { buildChartData, comboLabel } from './series';

const channel = (name: string, unit: string): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true });
const inputs = [channel('FZ', 'N'), channel('SA', 'rad'), channel('SR', '-'), channel('IA', 'rad'), channel('VX', 'm/s')];
const outputChannels: OutputChannel[] = [
  { name: 'FX', unit: 'N' },
  { name: 'FY', unit: 'N' },
  { name: 'MZ', unit: 'N*m' },
  { name: 'FLAG', unit: '-' },
];

const base: ChartSpec = {
  id: 'c',
  sweep: { variable: 'SA', start: -5, end: 5, points: 3 },
  steps: [],
  fixed: { FZ: 4000, SR: 0, IA: 0, VX: 16.7 },
  output: 'FY',
  uncombined: false,
};
const twoSteps: ChartSpec = {
  ...base,
  steps: [
    { variable: 'FZ', values: [3000, 4000], colourMode: 'hue' },
    { variable: 'IA', values: [0, 2, 4], colourMode: 'saturation' },
  ],
  fixed: { SR: 0, VX: 16.7 },
};

function outputsFor(rows: number): Float64Array {
  const data = new Float64Array(rows * outputChannels.length);
  for (let o = 0; o < outputChannels.length; o++) {
    for (let r = 0; r < rows; r++) {
      data[o * rows + r] = o * 1000 + r;
    }
  }
  return data;
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
  it('builds one series without steps, labelled by the output', () => {
    const batch = buildBatch(base, inputs);
    const data = buildChartData(base, batch, outputsFor(batch.rows), inputs, outputChannels, 'light');
    expect(data.series).toEqual([{ key: 's', label: 'FY', colour: 'oklch(0.575 0.163 255.5)' }]);
    expect(data.rows.map((r) => r.x)).toEqual([-5, 0, 5]);
    expect(data.rows.map((r) => r.s)).toEqual([1000, 1001, 1002]);
    expect(data.xLabel).toBe('SA [deg]');
    expect(data.yLabel).toBe('FY [N]');
  });

  it('uses the dark palette in the dark theme', () => {
    const batch = buildBatch(base, inputs);
    const data = buildChartData(base, batch, outputsFor(batch.rows), inputs, outputChannels, 'dark');
    expect(data.series[0].colour).toBe(toCss(SERIES_PALETTES.dark[0]));
  });

  it('builds one series per combination with unique keys and column-major outputs', () => {
    const batch = buildBatch(twoSteps, inputs);
    const data = buildChartData(twoSteps, batch, outputsFor(batch.rows), inputs, outputChannels, 'light');
    expect(data.series.map((s) => s.key)).toEqual(['s0_0', 's0_1', 's0_2', 's1_0', 's1_1', 's1_2']);
    expect(new Set(data.series.map((s) => s.key)).size).toBe(6);
    expect(data.series[4].label).toBe('FZ = 4000 N, IA = 2 deg');
    expect(data.rows[2]['s1_1']).toBe(1000 + 4 * 3 + 2);
    batch.combinations.forEach((combination, c) =>
      expect(data.series[c].colour).toBe(toCss(colourFor(combination, twoSteps.steps, 'light'))),
    );
  });

  it('labels moment and flag axes', () => {
    const batch = buildBatch({ ...base, output: 'MZ' }, inputs);
    const mzSpec = { ...base, output: 'MZ' };
    const mz = buildChartData(mzSpec, batch, outputsFor(batch.rows), inputs, outputChannels, 'light');
    expect(mz.yLabel).toBe('MZ [N·m]');
    const flagSpec = { ...base, output: 'FLAG' };
    const flag = buildChartData(flagSpec, batch, outputsFor(batch.rows), inputs, outputChannels, 'light');
    expect(flag.yLabel).toBe('FLAG');
  });

  it('converts angle outputs to degrees', () => {
    const channels: OutputChannel[] = [{ name: 'ANG', unit: 'rad' }];
    const batch = buildBatch({ ...base, output: 'ANG' }, inputs);
    const angSpec = { ...base, output: 'ANG' };
    const data = buildChartData(angSpec, batch, new Float64Array([Math.PI, 0, 0]), inputs, channels, 'light');
    expect(data.rows[0].s).toBeCloseTo(180, 10);
  });

  it('throws RangeError for an unknown output', () => {
    const batch = buildBatch(base, inputs);
    const unknown = { ...base, output: 'ZZ' };
    expect(() => buildChartData(unknown, batch, outputsFor(batch.rows), inputs, outputChannels, 'light')).toThrow(
      RangeError,
    );
  });
});
