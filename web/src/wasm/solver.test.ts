import { describe, expect, it, vi } from 'vitest';
import type { MfWasmModule, RawEntry, RawInspection, RawModel } from './mf_wasm.js';
import { inspectTir } from './solver';

vi.mock('./mf_wasm.js', () => ({ default: vi.fn() }));

const model: RawModel = {
  name: 'MF 5.2',
  fittyp: 6,
  useMode: null,
  tyreSide: 'LEFT',
  inputs: [],
  outputs: [],
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
      fake({ ok: true, entries, warnings: [], ignoredKeys: ['MASS', 'USER_SUB_ID'], model, loadError: null }),
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
      fake({ ok: true, entries, warnings: [], ignoredKeys: ['USER_SUB_ID'], model: null, loadError: null }),
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
