import { describe, expect, it } from 'vitest';
import {
  AXIS_GROUPS,
  axisChannel,
  axisDomain,
  axisKey,
  axisLabel,
  axisOptions,
  channelColumn,
  channelsOfKind,
  finiteExtent,
} from './axes';
import { buildBatch } from './batch';
import type { AxisRef, SweepSpec } from './chart-spec';
import type { InputChannel, ModelInfo } from './model';
import type { SweepSolution } from './solve';
import { niceExtent } from './ticks';

const channel = (name: string, unit: string): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true, negatedBy: name === 'FZ' ? 'VERTICAL' : name === 'SA' ? 'LATERAL' : 'NONE' });

const model: ModelInfo = {
  name: 'm',
  fittyp: null,
  useMode: null,
  tyreSide: 'LEFT',
  inputs: [channel('FZ', 'N'), channel('SA', 'rad'), channel('SR', '-')],
  outputs: [
    { name: 'FX', unit: 'N', negatedBy: 'NONE' },
    { name: 'FY', unit: 'N', negatedBy: 'VERTICAL' },
    { name: 'ANG', unit: 'rad', negatedBy: 'NONE' },
    { name: 'MZ', unit: 'N*m', negatedBy: 'VERTICAL' },
    { name: 'FLAG', unit: '-', negatedBy: 'NONE' },
  ],
  characteristics: [
    { name: 'DFY_DSA', unit: 'N/rad' },
    { name: 'SA_AT_PEAK_FY', unit: 'rad' },
    { name: 'SR_AT_PEAK_FX', unit: '-' },
    { name: 'DY', unit: 'N' },
  ],
  limits: [],
};

const sweep: SweepSpec = {
  id: 'sweep-1',
  name: 'Sweep 1',
  swept: { variable: 'SA', start: -15, end: 15, points: 3 },
  steps: [{ variable: 'FZ', values: [2000, 4000], colourMode: 'hue' }],
  fixed: { SR: 0.1 },
  uncombined: false,
};

describe('axisKey', () => {
  it('joins kind and name', () => {
    expect(axisKey({ kind: 'input', name: 'SA' })).toBe('input:SA');
    expect(axisKey({ kind: 'output', name: 'FY' })).toBe('output:FY');
  });
});

describe('axisLabel', () => {
  it.each([
    ['SA', 'rad', 'SA [deg]'],
    ['MZ', 'N*m', 'MZ [N·m]'],
    ['FX', 'N', 'FX [N]'],
    ['SR', '-', 'SR'],
    ['FLAG', '', 'FLAG'],
  ])('labels %s with %s as %s', (name, unit, expected) => {
    expect(axisLabel(name, unit)).toBe(expected);
  });
});

describe('channelsOfKind', () => {
  it('returns the model channel lists', () => {
    expect(channelsOfKind(model, 'input')).toBe(model.inputs);
    expect(channelsOfKind(model, 'output')).toBe(model.outputs);
    expect(channelsOfKind(model, 'characteristic')).toBe(model.characteristics);
  });
});

describe('AXIS_GROUPS', () => {
  it('lists the groups in display order', () => {
    expect(AXIS_GROUPS).toEqual([
      { kind: 'input', title: 'Inputs' },
      { kind: 'output', title: 'Outputs' },
      { kind: 'characteristic', title: 'Characteristics' },
    ]);
  });
});

describe('axisOptions', () => {
  it('lists inputs then outputs in model order', () => {
    const options = axisOptions(model);
    expect(options.map((o) => o.key)).toEqual([
      'input:FZ',
      'input:SA',
      'input:SR',
      'output:FX',
      'output:FY',
      'output:ANG',
      'output:MZ',
      'output:FLAG',
      'characteristic:DFY_DSA',
      'characteristic:SA_AT_PEAK_FY',
      'characteristic:SR_AT_PEAK_FX',
      'characteristic:DY',
    ]);
    expect(options[1]).toEqual({
      key: 'input:SA',
      ref: { kind: 'input', name: 'SA' },
      label: 'SA [deg]',
      description: 'Slip angle',
    });
    expect(options[7].label).toBe('FLAG');
    expect(options[7].description).toBe('');
    expect(options.slice(8).map((o) => o.label)).toEqual(['DFY_DSA [N/deg]', 'SA_AT_PEAK_FY [deg]', 'SR_AT_PEAK_FX', 'DY [N]']);
    expect(options[8].description).toBe('Instantaneous cornering stiffness');
  });
});

describe('axisChannel', () => {
  it('finds an input and an output', () => {
    expect(axisChannel(model, { kind: 'input', name: 'SA' })).toBe(model.inputs[1]);
    expect(axisChannel(model, { kind: 'output', name: 'FY' })).toBe(model.outputs[1]);
    expect(axisChannel(model, { kind: 'characteristic', name: 'DY' })).toBe(model.characteristics[3]);
  });

  it('throws RangeError for an unknown channel', () => {
    expect(() => axisChannel(model, { kind: 'input', name: 'QQ' })).toThrow(new RangeError("Unknown input 'QQ'"));
    expect(() => axisChannel(model, { kind: 'output', name: 'QQ' })).toThrow(new RangeError("Unknown output 'QQ'"));
    expect(() => axisChannel(model, { kind: 'characteristic', name: 'QQ' })).toThrow(
      new RangeError("Unknown characteristic 'QQ'"),
    );
  });
});

describe('channelColumn', () => {
  const batch = buildBatch(sweep, model.inputs);
  const outputs = new Float64Array(model.outputs.length * batch.rows);
  outputs.forEach((_, i) => {
    outputs[i] = i + 1;
  });
  outputs[2 * batch.rows] = Math.PI;
  const characteristics = new Float64Array(model.characteristics.length * batch.rows);
  characteristics.forEach((_, i) => {
    characteristics[i] = i + 1;
  });
  characteristics[0] = 180 / Math.PI;
  characteristics[batch.rows] = Math.PI;
  characteristics[batch.rows + 1] = Number.NaN;
  const solution: SweepSolution = { batch, outputs, characteristics };
  const column = (kind: AxisRef['kind'], name: string) => channelColumn(sweep, solution, model, { kind, name });

  it('returns the swept values exactly, without an SI round trip', () => {
    expect(column('input', 'SA')).toEqual([-15, 0, 15, -15, 0, 15]);
  });

  it('returns the step value of each combination', () => {
    expect(column('input', 'FZ')).toEqual([2000, 2000, 2000, 4000, 4000, 4000]);
  });

  it('returns the held value for a fixed input', () => {
    expect(column('input', 'SR')).toEqual([0.1, 0.1, 0.1, 0.1, 0.1, 0.1]);
  });

  it('returns an output column converted to display units', () => {
    expect(column('output', 'FY')).toEqual(Array.from({ length: 6 }, (_, r) => 6 + r + 1));
    expect(column('output', 'MZ')).toEqual(Array.from({ length: 6 }, (_, r) => 18 + r + 1));
    expect(column('output', 'ANG')[0]).toBeCloseTo(180, 9);
  });

  it('converts a characteristic column to display units', () => {
    expect(column('characteristic', 'DFY_DSA')[0]).toBeCloseTo(1, 12);
    expect(column('characteristic', 'DFY_DSA')[1]).toBeCloseTo((2 * Math.PI) / 180, 12);
    expect(column('characteristic', 'SA_AT_PEAK_FY')[0]).toBeCloseTo(180, 9);
    expect(column('characteristic', 'DY')).toEqual(Array.from({ length: 6 }, (_, r) => 18 + r + 1));
  });

  it('keeps an undefined peak slip as NaN, not 0', () => {
    const peak = column('characteristic', 'SA_AT_PEAK_FY');
    expect(Number.isNaN(peak[1])).toBe(true);
    expect(peak[2]).not.toBe(0);
  });

  it('throws RangeError for a characteristic of a sweep solved without them', () => {
    const plain: SweepSolution = { ...solution, characteristics: null };
    expect(() => channelColumn(sweep, plain, model, { kind: 'characteristic', name: 'DY' })).toThrow(
      new RangeError('The characteristics of sweep-1 were not solved'),
    );
  });

  it('throws RangeError for unknown channels', () => {
    expect(() => column('input', 'QQ')).toThrow(RangeError);
    expect(() => column('output', 'QQ')).toThrow(RangeError);
    expect(() => column('characteristic', 'QQ')).toThrow(RangeError);
  });
});

describe('finiteExtent', () => {
  it('returns min and max', () => {
    expect(finiteExtent([3, -1, 2])).toEqual([-1, 3]);
  });

  it('ignores non-finite values', () => {
    expect(finiteExtent([Number.NaN, 2, Infinity, -Infinity, 5])).toEqual([2, 5]);
  });

  it('falls back to [0, 1] without finite values', () => {
    expect(finiteExtent([])).toEqual([0, 1]);
    expect(finiteExtent([Number.NaN])).toEqual([0, 1]);
  });
});

describe('axisDomain', () => {
  it('keeps an input extent exact', () => {
    expect(axisDomain('input', [-15, 15])).toEqual([-15, 15]);
  });

  it('widens a constant input extent', () => {
    expect(axisDomain('input', [0, 0])).toEqual([-1, 1]);
  });

  it('rounds an output extent', () => {
    expect(axisDomain('output', [-4321, 5678])).toEqual(niceExtent(-4321, 5678, 8));
  });

  it('rounds a characteristic extent', () => {
    expect(axisDomain('characteristic', [1.3, 7.9])).toEqual(niceExtent(1.3, 7.9, 8));
  });
});
