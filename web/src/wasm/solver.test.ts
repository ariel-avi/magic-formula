import { describe, expect, it, vi } from 'vitest';
import type { MfWasmModule, RawEntry, RawIdentification, RawInspection, RawModel } from './mf_wasm.js';
import type { SweepBatch } from '@/domain/batch';
import { convertModel, inspectTir, solveBatch } from './solver';

vi.mock('./mf_wasm.js', () => ({ default: vi.fn() }));

const model: RawModel = {
  name: 'MF 5.2',
  fittyp: 6,
  useMode: null,
  tyreSide: 'LEFT',
  inputs: [],
  outputs: [],
  characteristics: [],
  limits: [],
};

function entry(section: string, key: string, line: number): RawEntry {
  return { section, key, value: '1', quoted: false, line };
}

function fake(raw: RawInspection): MfWasmModule {
  return { inspect: () => raw } as unknown as MfWasmModule;
}

const entries = [
  entry('UNITS', 'MASS', 1),
  entry('INERTIA', 'MASS', 2),
  entry('MODEL', 'USER_SUB_ID', 3),
  entry('MODEL', 'FITTYP', 4),
];

describe('inspectTir unused marking', () => {
  it('marks a unique ignored key unused and leaves ambiguous duplicates unmarked', () => {
    const result = inspectTir(
      fake({ ok: true, entries, warnings: [], ignoredKeys: ['MASS', 'USER_SUB_ID'], model, coordinates: null, loadError: null }),
      'x',
      'x.tir',
    );
    expect(result.ok).toBe(true);
    if (!result.ok) return;
    const used = Object.fromEntries(result.value.entries.map((e) => [`${e.section}.${e.key}`, e.used]));
    expect(used).toEqual({
      'UNITS.MASS': true,
      'INERTIA.MASS': true,
      'MODEL.USER_SUB_ID': false,
      'MODEL.FITTYP': true,
    });
  });

  it('marks nothing unused when no model loaded', () => {
    const result = inspectTir(
      fake({ ok: true, entries, warnings: [], ignoredKeys: ['USER_SUB_ID'], model: null, coordinates: null, loadError: null }),
      'x',
      'x.tir',
    );
    expect(result.ok && result.value.entries.every((e) => e.used)).toBe(true);
  });

  it('returns the adapter error for a syntax failure', () => {
    const error = { code: 'SYNTAX' as const, message: 'bad', line: 1, key: '' };
    expect(inspectTir(fake({ ok: false, error }), 'x', 'x.tir')).toEqual({ ok: false, error });
  });
});

const decision = {
  sign: 'NEGATIVE' as const,
  basis: 'DECIDED' as const,
  confidence: 1,
  positiveWeight: 0,
  negativeWeight: 2,
};
const identification: RawIdentification = {
  classification: 'SAE',
  vertical: decision,
  lateral: decision,
  effective: { vertical: 'NEGATIVE', lateral: 'NEGATIVE' },
  readings: [
    {
      name: 'dFy/dSA',
      meaning: 'cornering stiffness',
      unit: 'N/rad',
      axis: 'LATERAL',
      loadScale: 1,
      load: -4000,
      value: -50000,
      normalised: -12.5,
      status: 'VOTE',
      vote: -1,
      weight: 0.3,
    },
  ],
  referenceInput: [1, 2],
  warnings: [{ message: 'w', line: 0, key: 'K' }],
};

describe('inspectTir coordinates', () => {
  const inputs = [{ name: 'FZ', unit: 'N', minimum: 0, minimumInclusive: true, negatedBy: 'VERTICAL' as const }];
  const outputs = [{ name: 'FY', unit: 'N', negatedBy: 'VERTICAL' as const }];

  it('maps the identification and the channel signs', () => {
    const result = inspectTir(
      fake({
        ok: true,
        entries: [],
        warnings: [],
        ignoredKeys: [],
        model: { ...model, inputs, outputs },
        coordinates: identification,
        loadError: null,
      }),
      'x',
      'x.tir',
    );
    expect(result.ok).toBe(true);
    if (!result.ok) return;
    expect(result.value.coordinates).toEqual(identification);
    expect(result.value.model?.inputs[0]?.negatedBy).toBe('VERTICAL');
    expect(result.value.model?.outputs[0]?.negatedBy).toBe('VERTICAL');
  });

  it('keeps a missing identification as null', () => {
    const result = inspectTir(
      fake({ ok: true, entries: [], warnings: [], ignoredKeys: [], model: null, coordinates: null, loadError: null }),
      'x',
      'x.tir',
    );
    expect(result.ok && result.value.coordinates).toBeNull();
  });
});

describe('solveBatch', () => {
  const batch = { inputs: new Float64Array([1, 2]) } as unknown as SweepBatch;

  it('passes no system and returns the warnings', () => {
    const evaluate = vi.fn(() => ({
      ok: true,
      rows: 1,
      outputs: new Float64Array([3]),
      warnings: [{ message: 'FZ < 0', key: 'FZ', row: 0 }],
    }));
    const result = solveBatch({ evaluate } as unknown as MfWasmModule, 'text', batch, true, false);
    expect(evaluate).toHaveBeenCalledWith('text', batch.inputs, true);
    expect(result).toEqual({
      ok: true,
      value: {
        outputs: new Float64Array([3]),
        characteristics: null,
        warnings: [{ message: 'FZ < 0', key: 'FZ', row: 0 }],
      },
    });
  });

  it('keeps the failing row on an error', () => {
    const error = { code: 'NUMERICAL' as const, message: 'bad', line: 0, key: '' };
    const mf = { evaluate: () => ({ ok: false, error, row: 4 }) } as unknown as MfWasmModule;
    expect(solveBatch(mf, 'text', batch, false, false)).toEqual({ ok: false, error: { ...error, row: 4 } });
  });

  it('calls evaluateWithCharacteristics when asked and maps every field', () => {
    const evaluate = vi.fn();
    const evaluateWithCharacteristics = vi.fn(() => ({
      ok: true,
      rows: 1,
      outputs: new Float64Array([3]),
      characteristics: new Float64Array([4, Number.NaN]),
      warnings: [{ message: 'w', key: 'FZ', row: 0 }],
    }));
    const mf = { evaluate, evaluateWithCharacteristics } as unknown as MfWasmModule;
    const result = solveBatch(mf, 'text', batch, true, true);
    expect(evaluate).not.toHaveBeenCalled();
    expect(evaluateWithCharacteristics).toHaveBeenCalledWith('text', batch.inputs, true);
    expect(result).toEqual({
      ok: true,
      value: {
        outputs: new Float64Array([3]),
        characteristics: new Float64Array([4, Number.NaN]),
        warnings: [{ message: 'w', key: 'FZ', row: 0 }],
      },
    });
  });

  it.each([4, null])('keeps the row %s of a characteristics failure', (row) => {
    const error = { code: 'NUMERICAL' as const, message: 'bad', line: 0, key: 'DFY_DSA' };
    const mf = { evaluateWithCharacteristics: () => ({ ok: false, error, row }) } as unknown as MfWasmModule;
    expect(solveBatch(mf, 'text', batch, false, true)).toEqual({ ok: false, error: { ...error, row } });
  });
});

describe('convertModel', () => {
  const applied = { vertical: true, lateral: false };
  const changes = [{ section: 'VERTICAL', key: 'FNOMIN', line: 5, before: '4000', after: '-4000' }];

  it('passes the arguments and maps a successful conversion', () => {
    const convertModelRaw = vi.fn(() => ({ ok: true, text: 'new', applied, changes }));
    const mf = { convertModel: convertModelRaw } as unknown as MfWasmModule;
    expect(convertModel(mf, 'old', 'x.tir', 'SAE')).toEqual({ ok: true, value: { text: 'new', applied, changes } });
    expect(convertModelRaw).toHaveBeenCalledWith('old', 'x.tir', 'SAE');
  });

  it('returns the adapter error', () => {
    const error = { code: 'INVALID_ARGUMENT' as const, message: 'bad', line: 0, key: '' };
    const mf = { convertModel: () => ({ ok: false, error }) } as unknown as MfWasmModule;
    expect(convertModel(mf, 'old', 'x.tir', 'ISO')).toEqual({ ok: false, error });
  });
});
