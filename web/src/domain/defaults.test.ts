import { describe, expect, it } from 'vitest';
import { defaultChartSpec, defaultFixedValue, defaultSweep } from './defaults';
import type { InputChannel, ModelInfo, TirEntry } from './model';

const channel = (name: string, unit = '-'): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true });
const entry = (key: string, value: string): TirEntry => ({ section: 'S', key, value, quoted: false, line: 1, used: true });

describe('defaultFixedValue', () => {
  it('takes FZ from FNOMIN and falls back to 4000', () => {
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '5500')])).toBe(5500);
    expect(defaultFixedValue(channel('FZ'), [])).toBe(4000);
    expect(defaultFixedValue(channel('FZ'), [entry('FNOMIN', '0')])).toBe(4000);
  });

  it('takes VX from LONGVL and falls back to 16.7', () => {
    expect(defaultFixedValue(channel('VX'), [entry('LONGVL', '11')])).toBe(11);
    expect(defaultFixedValue(channel('VX'), [])).toBe(16.7);
  });

  it.each(['SA', 'SR', 'IA', 'P'])('uses 0 for %s', (name) => {
    expect(defaultFixedValue(channel(name), [])).toBe(0);
  });
});

describe('defaultSweep', () => {
  const entries = [entry('FNOMIN', '1000')];

  it('has presentation ranges per channel', () => {
    expect(defaultSweep(channel('SA'), entries)).toEqual({ variable: 'SA', start: -15, end: 15, points: 61 });
    expect(defaultSweep(channel('SR'), entries)).toEqual({ variable: 'SR', start: -0.3, end: 0.3, points: 61 });
    expect(defaultSweep(channel('IA'), entries)).toEqual({ variable: 'IA', start: -6, end: 6, points: 25 });
    expect(defaultSweep(channel('VX'), entries)).toEqual({ variable: 'VX', start: 1, end: 40, points: 40 });
  });

  it('scales the FZ range by the nominal load', () => {
    const sweep = defaultSweep(channel('FZ'), entries);
    expect(sweep.start).toBeCloseTo(200, 9);
    expect(sweep.end).toBe(2000);
    expect(sweep.points).toBe(37);
  });

  it('falls back to 0..1 for other channels', () => {
    expect(defaultSweep(channel('P'), entries)).toEqual({ variable: 'P', start: 0, end: 1, points: 11 });
  });
});

describe('defaultChartSpec', () => {
  const pac: ModelInfo = {
    name: 'PAC2002',
    fittyp: null,
    useMode: null,
    tyreSide: 'LEFT',
    inputs: ['FZ', 'SA', 'SR', 'IA', 'VX'].map((name) => channel(name)),
    outputs: [
      { name: 'FX', unit: 'N' },
      { name: 'FY', unit: 'N' },
    ],
    limits: [],
  };

  it('plots FY against SA with an FZ hue step', () => {
    const spec = defaultChartSpec(pac, [entry('FNOMIN', '4000')], 'c1');
    expect(spec.id).toBe('c1');
    expect(spec.sweep.variable).toBe('SA');
    expect(spec.steps).toEqual([{ variable: 'FZ', values: [2000, 4000, 6000], colourMode: 'hue' }]);
    expect(spec.fixed).toEqual({ FZ: 4000, SA: 0, SR: 0, IA: 0, VX: 16.7 });
    expect(spec.output).toBe('FY');
    expect(spec.uncombined).toBe(false);
  });

  it('uses the first input and output for an unknown layout', () => {
    const other: ModelInfo = { ...pac, inputs: [channel('P'), channel('VX')], outputs: [{ name: 'Q', unit: 'N' }] };
    const spec = defaultChartSpec(other, [], 'c2');
    expect(spec.sweep.variable).toBe('P');
    expect(spec.steps).toEqual([]);
    expect(spec.output).toBe('Q');
  });

  it('falls back when FY is missing', () => {
    const noFy: ModelInfo = { ...pac, outputs: [{ name: 'FX', unit: 'N' }] };
    const spec = defaultChartSpec(noFy, [], 'c3');
    expect(spec.sweep.variable).toBe('FZ');
    expect(spec.output).toBe('FX');
  });

  it('falls back when FZ is missing', () => {
    const noFz: ModelInfo = { ...pac, inputs: [channel('SA')] };
    expect(defaultChartSpec(noFz, [], 'c4').steps).toEqual([]);
  });
});
