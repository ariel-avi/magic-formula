import { describe, expect, it } from 'vitest';
import { needsCharacteristics } from './chart';
import { defaultChartSpec, defaultFixedValue, defaultSweepSpec, defaultSweptRange } from './defaults';
import type { InputChannel, ModelInfo, TirEntry } from './model';

const channel = (name: string, unit = '-'): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true, negatedBy: name === 'FZ' ? 'VERTICAL' : name === 'SA' ? 'LATERAL' : 'NONE' });
const entry = (key: string, value: string): TirEntry => ({ section: 'S', key, value, quoted: false, line: 1, used: true });

describe('defaultFixedValue', () => {
  it('takes FZ from FNOMIN and falls back to 4000', () => {
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '5500')])).toBe(5500);
    expect(defaultFixedValue(channel('FZ'), [])).toBe(4000);
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '0')])).toBe(4000);
  });

  it('keeps the sign of FNOMIN', () => {
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '-4000')])).toBe(-4000);
  });

  it('scales FNOMIN by a positive LFZO and ignores any other', () => {
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '4000'), entry('LFZO', '1.1')])).toBeCloseTo(4400, 9);
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '4000'), entry('LFZO', '0')])).toBe(4000);
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '4000'), entry('LFZO', '-2')])).toBe(4000);
  });

  it('takes VX from LONGVL and falls back to 16.7', () => {
    expect(defaultFixedValue(channel('VX'), [entry('LONGVL', '11')])).toBe(11);
    expect(defaultFixedValue(channel('VX'), [])).toBe(16.7);
  });

  it.each(['SA', 'SR', 'IA', 'TS'])('uses 0 for %s', (name) => {
    expect(defaultFixedValue(channel(name), [])).toBe(0);
  });
});

describe('defaultSweptRange', () => {
  const entries = [entry('FNOMIN', '1000')];

  it('has presentation ranges per channel', () => {
    expect(defaultSweptRange(channel('SA'), entries)).toEqual({ variable: 'SA', start: -15, end: 15, points: 61 });
    expect(defaultSweptRange(channel('SR'), entries)).toEqual({ variable: 'SR', start: -0.3, end: 0.3, points: 61 });
    expect(defaultSweptRange(channel('IA'), entries)).toEqual({ variable: 'IA', start: -6, end: 6, points: 25 });
    expect(defaultSweptRange(channel('VX'), entries)).toEqual({ variable: 'VX', start: 1, end: 40, points: 40 });
  });

  it('scales the FZ range by the nominal load', () => {
    const sweep = defaultSweptRange(channel('FZ'), entries);
    expect(sweep.start).toBeCloseTo(200, 9);
    expect(sweep.end).toBe(2000);
    expect(sweep.points).toBe(37);
  });

  it('keeps the FZ range ascending for a negative nominal load', () => {
    expect(defaultSweptRange(channel('FZ'), [entry('FNOMIN', '-4000')])).toEqual({
      variable: 'FZ',
      start: -8000,
      end: -800,
      points: 37,
    });
  });

  it('falls back to 0..1 for other channels', () => {
    expect(defaultSweptRange(channel('UNKNOWN'), entries)).toEqual({ variable: 'UNKNOWN', start: 0, end: 1, points: 11 });
  });
});

const pac: ModelInfo = {
  name: 'PAC2002',
  fittyp: null,
  useMode: null,
  tyreSide: 'LEFT',
  inputs: ['FZ', 'SA', 'SR', 'IA', 'VX'].map((name) => channel(name)),
  outputs: [
    { name: 'FX', unit: 'N', negatedBy: 'NONE' },
    { name: 'FY', unit: 'N', negatedBy: 'VERTICAL' },
  ],
  characteristics: [],
  limits: [],
};

describe('defaultSweepSpec', () => {
  it('sweeps SA with an FZ hue step at 0.5, 1 and 1.5 FZ0', () => {
    const fz0 = 4000;
    const spec = defaultSweepSpec(pac, [entry('FNOMIN', String(fz0))], 'sweep-7', 'Front');
    expect(spec.id).toBe('sweep-7');
    expect(spec.name).toBe('Front');
    expect(spec.swept).toEqual({ variable: 'SA', start: -15, end: 15, points: 61 });
    expect(spec.steps).toEqual([{ variable: 'FZ', values: [0.5 * fz0, fz0, 1.5 * fz0], colourMode: 'hue' }]);
    expect(spec.fixed).toEqual({ FZ: 4000, SA: 0, SR: 0, IA: 0, VX: 16.7 });
    expect(spec.uncombined).toBe(false);
  });

  it('uses signed steps for a negative nominal load', () => {
    expect(defaultSweepSpec(pac, [entry('FNOMIN', '-4000')], 's', 'S').steps[0].values).toEqual([-2000, -4000, -6000]);
  });

  it('sweeps the first input without steps when SA is missing', () => {
    const noSa: ModelInfo = { ...pac, inputs: [channel('FZ'), channel('SR')] };
    const spec = defaultSweepSpec(noSa, [], 'a', 'A');
    expect(spec.swept.variable).toBe('FZ');
    expect(spec.steps).toEqual([]);
    expect(Object.keys(spec.fixed)).toEqual(['FZ', 'SR']);
  });

  it('sweeps the first input without steps when FZ is missing', () => {
    const noFz: ModelInfo = { ...pac, inputs: [channel('SA')] };
    const spec = defaultSweepSpec(noFz, [], 'a', 'A');
    expect(spec.swept.variable).toBe('SA');
    expect(spec.steps).toEqual([]);
  });
});

describe('defaultChartSpec', () => {
  it('plots FY against SA with one default sweep', () => {
    const chart = defaultChartSpec(pac, [entry('FNOMIN', '4000')], 'c1');
    expect(chart.id).toBe('c1');
    expect(chart.sweeps).toHaveLength(1);
    expect(chart.sweeps[0].id).toBe('sweep-1');
    expect(chart.sweeps[0].name).toBe('Sweep 1');
    expect(chart.sweeps[0].swept.variable).toBe('SA');
    expect(chart.x).toEqual({ kind: 'input', name: 'SA' });
    expect(chart.y).toEqual({ kind: 'output', name: 'FY' });
  });

  it('does not need characteristics', () => {
    expect(needsCharacteristics(defaultChartSpec(pac, [entry('FNOMIN', '4000')], 'c1'))).toBe(false);
  });

  it('keeps the SA sweep with FZ steps and plots the first output when FY is missing', () => {
    const noFy: ModelInfo = { ...pac, outputs: [{ name: 'FX', unit: 'N', negatedBy: 'NONE' }] };
    const chart = defaultChartSpec(noFy, [], 'c3');
    expect(chart.y).toEqual({ kind: 'output', name: 'FX' });
    expect(chart.sweeps[0].swept.variable).toBe('SA');
    expect(chart.sweeps[0].steps).toHaveLength(1);
  });

  it('uses the first input and output for an unknown layout', () => {
    const other: ModelInfo = { ...pac, inputs: [channel('P'), channel('VX')], outputs: [{ name: 'Q', unit: 'N', negatedBy: 'NONE' }] };
    const chart = defaultChartSpec(other, [], 'c2');
    expect(chart.x).toEqual({ kind: 'input', name: 'P' });
    expect(chart.y).toEqual({ kind: 'output', name: 'Q' });
  });
});

describe('MF6.1 defaults', () => {
  it('uses canonical load scaling, nominal pressure and zero turn slip', () => {
    const values = [entry('FNOMIN', '5000'), entry('LFZ0', '1.2'), entry('NOMPRES', '180000')];
    expect(defaultFixedValue(channel('FZ'), values)).toBe(6000);
    expect(defaultFixedValue(channel('P'), values)).toBe(180000);
    expect(defaultFixedValue(channel('P'), [])).toBe(220000);
    expect(defaultFixedValue(channel('TS'), values)).toBe(0);
    expect(defaultSweptRange(channel('P'), values)).toEqual({ variable: 'P', start: 100000, end: 300000, points: 21 });
    expect(defaultSweptRange(channel('TS'), values)).toEqual({ variable: 'TS', start: -0.1, end: 0.1, points: 41 });
  });
});
