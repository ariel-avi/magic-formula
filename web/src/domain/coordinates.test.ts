import { describe, expect, it } from 'vitest';
import { buildBatch } from './batch';
import type { ChartSpec, SweepSpec } from './chart-spec';
import {
  activeSystem,
  convertSpec,
  describeConversion,
  describeIdentification,
  formatBatchWarning,
  readingRows,
  sceneSystem,
} from './coordinates';
import type {
  AxisDecision,
  AxisSign,
  Classification,
  Conversion,
  CoordinateIdentification,
  IndicatorReading,
  InputChannel,
  TirChange,
} from './model';

const input = (name: string, unit: string): InputChannel => ({
  name,
  unit,
  minimum: 0,
  minimumInclusive: true,
  negatedBy: name === 'FZ' ? 'VERTICAL' : name === 'SA' ? 'LATERAL' : 'NONE',
});
const inputs = [input('FZ', 'N'), input('SA', 'rad'), input('SR', '-'), input('IA', 'rad'), input('VX', 'm/s')];

const decision = (sign: AxisSign, confidence = 1): AxisDecision => ({
  sign,
  basis: sign === 'UNDETERMINED' ? 'INSUFFICIENT_EVIDENCE' : 'DECIDED',
  confidence,
  positiveWeight: 0,
  negativeWeight: 0,
});

const identification = (
  classification: Classification,
  vertical: AxisSign,
  lateral: AxisSign,
  readings: IndicatorReading[] = [],
): CoordinateIdentification => ({
  classification,
  vertical: decision(vertical),
  lateral: decision(lateral),
  effective: {
    vertical: vertical === 'NEGATIVE' ? 'NEGATIVE' : 'POSITIVE',
    lateral: lateral === 'NEGATIVE' ? 'NEGATIVE' : 'POSITIVE',
  },
  readings,
  referenceInput: [],
  warnings: [],
});

const ISO = identification('ISO', 'POSITIVE', 'POSITIVE');
const SAE = identification('SAE', 'NEGATIVE', 'NEGATIVE');

describe('activeSystem', () => {
  it('is null without identification or for a mixed model', () => {
    expect(activeSystem(null)).toBeNull();
    expect(activeSystem(identification('MIXED', 'POSITIVE', 'NEGATIVE'))).toBeNull();
  });
  it('is the classification otherwise', () => {
    expect(activeSystem(ISO)).toBe('ISO');
    expect(activeSystem(SAE)).toBe('SAE');
  });
});

describe('sceneSystem', () => {
  it('is ISO without identification', () => {
    expect(sceneSystem(null)).toBe('ISO');
  });
  it('is the classification for ISO and SAE', () => {
    expect(sceneSystem(ISO)).toBe('ISO');
    expect(sceneSystem(SAE)).toBe('SAE');
  });
  it('follows the vertical side for a mixed model', () => {
    expect(sceneSystem(identification('MIXED', 'POSITIVE', 'NEGATIVE'))).toBe('ISO');
    expect(sceneSystem(identification('MIXED', 'NEGATIVE', 'POSITIVE'))).toBe('SAE');
  });
});

describe('describeIdentification', () => {
  it('reports an unidentified model', () => {
    expect(describeIdentification(null)).toBe('Coordinate system not identified');
  });
  it('describes ISO with both axes decided', () => {
    expect(describeIdentification(ISO)).toBe('ISO (vertical ISO, confidence 1.00; lateral ISO, confidence 1.00)');
  });
  it('marks an undetermined axis as assumed', () => {
    const id = { ...identification('SAE', 'NEGATIVE', 'UNDETERMINED'), effective: { vertical: 'NEGATIVE', lateral: 'NEGATIVE' } } as const;
    expect(describeIdentification(id)).toBe(
      'SAE (vertical SAE, confidence 1.00; lateral SAE, confidence 1.00 (assumed))',
    );
  });
  it('describes a mixed model', () => {
    expect(describeIdentification(identification('MIXED', 'POSITIVE', 'NEGATIVE'))).toBe(
      'Mixed: vertical ISO (a loaded tire has FZ > 0), lateral SAE (dFy/dSA < 0)',
    );
  });
  it('describes a mixed model with an assumed axis', () => {
    const id = { ...identification('MIXED', 'UNDETERMINED', 'POSITIVE'), effective: { vertical: 'NEGATIVE', lateral: 'POSITIVE' } } as const;
    expect(describeIdentification(id)).toBe(
      'Mixed: vertical SAE (a loaded tire has FZ < 0) (assumed), lateral ISO (dFy/dSA > 0)',
    );
  });
});

const reading = (over: Partial<IndicatorReading> = {}): IndicatorReading => ({
  name: 'dFy/dSA',
  meaning: 'cornering stiffness',
  unit: 'N/rad',
  axis: 'LATERAL',
  loadScale: 1,
  load: 4000,
  value: 51234.5678,
  normalised: 12.8,
  status: 'VOTE',
  vote: 1,
  weight: 0.25,
  ...over,
});

describe('readingRows', () => {
  it('is empty without identification', () => {
    expect(readingRows(null)).toEqual([]);
  });
  it('formats finite readings', () => {
    const rows = readingRows(identification('ISO', 'POSITIVE', 'POSITIVE', [reading(), reading({ loadScale: 0.5, vote: -1 })]));
    expect(rows[0]).toEqual({
      key: 'dFy/dSA@1',
      name: 'dFy/dSA',
      axis: 'LATERAL',
      scale: '1×',
      value: '5.123e+4 N/rad',
      status: 'VOTE',
      vote: '+1',
      weight: '0.250',
    });
    expect(rows[1].key).toBe('dFy/dSA@0.5');
    expect(rows[1].scale).toBe('0.5×');
    expect(rows[1].vote).toBe('-1');
  });
  it('shows a dash for a non-finite value and 0 for an abstaining vote', () => {
    const rows = readingRows(
      identification('ISO', 'POSITIVE', 'POSITIVE', [reading({ value: NaN, vote: 0, status: 'NOT_FINITE', weight: 0 })]),
    );
    expect(rows[0].value).toBe('—');
    expect(rows[0].vote).toBe('0');
    expect(rows[0].weight).toBe('0.000');
  });
});

const sweep = (over: Partial<SweepSpec> = {}): SweepSpec => ({
  id: 'sweep-1',
  name: 'Sweep 1',
  swept: { variable: 'SA', start: -15, end: 15, points: 61 },
  steps: [{ variable: 'FZ', values: [2000, 4000, 6000], colourMode: 'hue' }],
  fixed: { FZ: 4000, SA: 0, SR: 0.1, IA: 2, VX: 16.7 },
  uncombined: true,
  ...over,
});
const chart = (sweeps: SweepSpec[]): ChartSpec => ({
  id: 'chart',
  sweeps,
  x: { kind: 'input', name: 'SA' },
  y: { kind: 'output', name: 'FY' },
});

describe('convertSpec', () => {
  it('returns the same object when no vertical transform applies', () => {
    const spec = chart([sweep()]);
    expect(convertSpec(spec, inputs, { vertical: false, lateral: false })).toBe(spec);
    expect(convertSpec(spec, inputs, { vertical: false, lateral: true })).toBe(spec);
  });

  it('negates FZ values and keeps the rest', () => {
    const spec = chart([sweep()]);
    const out = convertSpec(spec, inputs, { vertical: true, lateral: false }).sweeps[0];
    expect(out.fixed).toEqual({ FZ: -4000, SA: 0, SR: 0.1, IA: 2, VX: 16.7 });
    expect(out.steps[0].values).toEqual([-2000, -4000, -6000]);
    expect(out.steps[0].colourMode).toBe('hue');
    expect(out.swept).toEqual({ variable: 'SA', start: -15, end: 15, points: 61 });
    expect(out.id).toBe('sweep-1');
    expect(out.uncombined).toBe(true);
  });

  it('maps a swept FZ range to an ascending negative range and leaves other steps alone', () => {
    const spec = chart([
      sweep({
        swept: { variable: 'FZ', start: 800, end: 8000, points: 37 },
        steps: [{ variable: 'IA', values: [1, 2], colourMode: 'saturation' }],
      }),
    ]);
    const out = convertSpec(spec, inputs, { vertical: true, lateral: true }).sweeps[0];
    expect(out.swept).toEqual({ variable: 'FZ', start: -8000, end: -800, points: 37 });
    expect(out.steps[0].values).toEqual([1, 2]);
  });
});

const change = (key: string): TirChange => ({ section: 'S', key, line: 1, before: '1', after: '-1' });
const conversion = (vertical: boolean, lateral: boolean, keys: string[]): Conversion => ({
  text: '',
  applied: { vertical, lateral },
  changes: keys.map(change),
});

describe('describeConversion', () => {
  it('reports no change', () => {
    expect(describeConversion('ISO', conversion(false, false, []))).toBe('Already ISO: no value changed sign');
  });
  it('uses the singular for one value', () => {
    expect(describeConversion('SAE', conversion(true, false, ['FNOMIN']))).toBe(
      'Converted to SAE: 1 value changed sign (FNOMIN)',
    );
  });
  it('lists the changed keys', () => {
    expect(describeConversion('ISO', conversion(false, true, ['A', 'B', 'C', 'D']))).toBe(
      'Converted to ISO: 4 values changed sign (A, B, C, D)',
    );
  });
  it('truncates after six keys', () => {
    expect(describeConversion('SAE', conversion(true, true, ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H']))).toBe(
      'Converted to SAE: 8 values changed sign (A, B, C, D, E, F, …)',
    );
  });
});

describe('formatBatchWarning', () => {
  it('appends the location of the first row', () => {
    const s = sweep({ fixed: {} });
    const batch = buildBatch(s, inputs);
    expect(formatBatchWarning(s, batch, { message: 'lift-off', key: 'FZ', row: 0 })).toBe(
      'lift-off (first at FZ = 2000, SA = -15)',
    );
  });
});
