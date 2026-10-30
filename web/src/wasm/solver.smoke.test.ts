import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import createMfModule, { type MfWasmModule, type RawModel } from './mf_wasm.js';

const dataDir = '../../../tests/data/';
const readData = (name: string): string => readFileSync(new URL(dataDir + name, import.meta.url), 'utf8');

// Tolerance of tests/module/mfeval_reference_test.cpp: |ours - ref| <= tol * (scale + max(|ours|, |ref|)).
const REL_TOL = 1e-10;
const FNOMIN = 4000;
const UNLOADED_RADIUS = 0.3;

interface ReferenceRow {
  line: number;
  values: Record<string, number>;
}

function readReference(): ReferenceRow[] {
  const lines = readData('reference/mf52_mfeval.csv').trim().split('\n');
  const header = lines[0]!.split(',');
  return lines.slice(1).map((text, index) => ({
    line: index + 2,
    values: Object.fromEntries(text.split(',').map((cell, k) => [header[k]!, Number(cell)])),
  }));
}

function solve(mf: MfWasmModule, text: string, model: RawModel, rows: Record<string, number>[]): Float64Array {
  const data = new Float64Array(rows.length * model.inputs.length);
  model.inputs.forEach((channel, k) => {
    rows.forEach((row, r) => {
      data[k * rows.length + r] = row[channel.name] ?? 0;
    });
  });
  const result = mf.evaluate(text, data, false);
  expect(result.ok).toBe(true);
  if (!result.ok) throw new Error(result.error.message);
  return result.outputs;
}

function loadModel(mf: MfWasmModule, text: string, fileName: string): RawModel {
  const inspection = mf.inspect(text, fileName);
  expect(inspection.ok).toBe(true);
  if (!inspection.ok || inspection.model === null) throw new Error('model did not load');
  return inspection.model;
}

describe('wasm solver smoke', () => {
  it('loads a .tir, solves a slip-angle sweep and returns finite outputs', async () => {
    const mf = await createMfModule();
    const text = readData('pac2002_S.tir');
    const model = loadModel(mf, text, 'pac2002_S.tir');
    expect(model.inputs.length).toBeGreaterThan(0);
    expect(model.outputs.length).toBeGreaterThan(0);

    const rows = Array.from({ length: 21 }, (_, r) => ({ SA: ((r - 10) / 10) * 0.2, FZ: 4000, VX: 16.6 }));
    const outputs = solve(mf, text, model, rows);
    expect(outputs.length).toBe(rows.length * model.outputs.length);
    expect(Array.from(outputs).every(Number.isFinite)).toBe(true);
    expect(outputs.some((v) => v !== 0)).toBe(true);
  });

  it('agrees with the committed MFeval reference rows of mf52_S52.tir', async () => {
    const mf = await createMfModule();
    const text = readData('mf52_S52.tir');
    const model = loadModel(mf, text, 'mf52_S52.tir');
    const reference = readReference();
    expect(reference).toHaveLength(190);

    // Pure longitudinal, pure lateral, combined slip and camber rows (CSV line numbers, header = line 1).
    const picked = [10, 14, 15, 33, 43, 44, 46, 87, 91].map((line) => reference.find((r) => r.line === line)!);
    const outputs = solve(mf, text, model, picked.map((r) => r.values));
    const rowCount = picked.length;
    const scales: Record<string, number> = {
      FX: FNOMIN,
      FY: FNOMIN,
      MZ: FNOMIN * UNLOADED_RADIUS,
      MX: FNOMIN * UNLOADED_RADIUS,
      MY: FNOMIN * UNLOADED_RADIUS,
    };
    model.outputs.forEach((channel, k) => {
      const scale = scales[channel.name];
      if (scale === undefined) return;
      picked.forEach((row, r) => {
        const ours = outputs[k * rowCount + r]!;
        const expected = row.values[channel.name]!;
        const bound = REL_TOL * (scale + Math.max(Math.abs(ours), Math.abs(expected)));
        expect(Math.abs(ours - expected), `${channel.name} at CSV line ${row.line}`).toBeLessThanOrEqual(bound);
      });
    });
    expect(model.outputs.map((c) => c.name)).toEqual(expect.arrayContaining(['FX', 'FY', 'MZ']));
  });

  it('changes the solved lateral force in the expected direction after a replaceValue edit', async () => {
    const mf = await createMfModule();
    const text = readData('mf52_S52.tir');
    const model = loadModel(mf, text, 'mf52_S52.tir');
    // Near-saturation point (CSV line 46): Fy tends to mu_y * Fz, so doubling PDY1 roughly doubles |Fy|.
    const point = readReference().find((r) => r.line === 46)!.values;
    const fyIndex = model.outputs.findIndex((c) => c.name === 'FY');
    expect(fyIndex).toBeGreaterThanOrEqual(0);

    const before = solve(mf, text, model, [point])[fyIndex]!;
    const edited = mf.replaceValue(text, 'LATERAL_COEFFICIENTS', 'PDY1', '2.0');
    expect(edited.ok).toBe(true);
    if (!edited.ok) return;
    const after = solve(mf, edited.text, model, [point])[fyIndex]!;

    expect(Math.abs(after)).toBeGreaterThan(Math.abs(before));
    // Fy = D*sin(...) + SVy: D scales with PDY1 but the vertical shift does not, so the ratio is near (not exactly) 2.
    expect(Math.abs(after) / Math.abs(before)).toBeGreaterThan(1.5);
    expect(Math.abs(after) / Math.abs(before)).toBeLessThan(2.5);
    expect(Math.sign(after)).toBe(Math.sign(before));
  });

  describe('characteristics', () => {
    // Expected values: test-data.md. Order: DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY, DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX.
    const NAMES = ['DFY_DSA', 'DFX_DSR', 'FY_OVER_SA', 'FX_OVER_SR', 'DY', 'DX', 'SA_AT_PEAK_FY', 'SR_AT_PEAK_FX'];
    const UNITS = ['N/rad', 'N', 'N/rad', 'N', 'N', 'N', 'rad', '-'];
    const PEAKS = new Set(['SA_AT_PEAK_FY', 'SR_AT_PEAK_FX']);
    const near = (actual: number, expected: number, name: string, what: string): void => {
      const rel = PEAKS.has(name) ? 1e-10 : 1e-12;
      expect(Math.abs(actual - expected), `${what} ${name}`).toBeLessThanOrEqual(rel * Math.abs(expected));
    };
    const batchOf = (model: RawModel, rows: Record<string, number>[]): Float64Array => {
      const data = new Float64Array(rows.length * model.inputs.length);
      model.inputs.forEach((channel, k) => {
        rows.forEach((row, r) => {
          data[k * rows.length + r] = row[channel.name] ?? 0;
        });
      });
      return data;
    };
    const characteristicsOf = (mf: MfWasmModule, text: string, model: RawModel, rows: Record<string, number>[]) => {
      const result = mf.evaluateWithCharacteristics(text, batchOf(model, rows), false);
      expect(result.ok).toBe(true);
      if (!result.ok) throw new Error(result.error.message);
      return result;
    };
    const row = (index: number, rows: number, values: Float64Array): number[] =>
      NAMES.map((_, c) => values[c * rows + index]!);
    const at = (FZ: number, SA: number, SR: number) => ({ FZ, SA, SR, IA: 0, VX: 10 });

    it('lists the eight characteristic channels in order', async () => {
      const mf = await createMfModule();
      const model = loadModel(mf, readData('pac2002_S.tir'), 'pac2002_S.tir');
      expect(model.characteristics.map((c) => c.name)).toEqual(NAMES);
      expect(model.characteristics.map((c) => c.unit)).toEqual(UNITS);
    });

    it('matches the reference characteristics and the plain outputs', async () => {
      const mf = await createMfModule();
      const text = readData('pac2002_S.tir');
      const model = loadModel(mf, text, 'pac2002_S.tir');
      const rows = [at(4000, 0, 0.1), at(4000, 0.05, 0), at(4000, 0, 0), at(4000, -0.05, 0), at(4000, 0, -0.1)];
      const expected = [
        [-39162.83710434, 5151.9006238, -39162.83710434, 39354.79640314, 4000, 4000, 0.2475646539106, 0.1299038105893],
        [-34707.6921758, 71554.17526807, -47527.96304983, 71554.17526807, 4000, 4000, 0.2475646539106, 0.1299038105893],
        [-55384.61537396, 79999.99998667, -55384.61537396, 79999.99998667, 4000, 4000, 0.2475646539106, 0.1299038105893],
        [-34707.6921758, 71554.17526807, -47527.96304983, 71554.17526807, 4000, 4000, -0.2475646539106, 0.1299038105893],
        [-39162.83710434, 5151.9006238, -39162.83710434, 39354.79640314, 4000, 4000, 0.2475646539106, -0.1299038105893],
      ];
      const result = characteristicsOf(mf, text, model, rows);
      const plain = mf.evaluate(text, batchOf(model, rows), false);
      expect(plain.ok).toBe(true);
      if (!plain.ok) return;
      expect(Array.from(result.outputs)).toEqual(Array.from(plain.outputs));
      expect(result.characteristics).toHaveLength(rows.length * NAMES.length);
      expected.forEach((values, r) => {
        const actual = row(r, rows.length, result.characteristics);
        values.forEach((value, c) => near(actual[c]!, value, NAMES[c]!, `row ${r}`));
      });
    });

    it('returns zero slopes and undefined peaks without contact', async () => {
      const mf = await createMfModule();
      const text = readData('pac2002_S.tir');
      const model = loadModel(mf, text, 'pac2002_S.tir');
      const rows = [at(0, 0.05, 0.1), at(0, 0, 0)];
      const result = characteristicsOf(mf, text, model, rows);
      rows.forEach((_, r) => {
        const values = row(r, rows.length, result.characteristics);
        [0, 1, 2, 3, 4, 5].forEach((c) => expect(Object.is(values[c], 0), `${NAMES[c]} row ${r}`).toBe(true));
        expect(Number.isNaN(values[6])).toBe(true);
        expect(Number.isNaN(values[7])).toBe(true);
      });
    });

    it('mirrors the lateral-conversion characteristics', async () => {
      const mf = await createMfModule();
      const text = readData('pac2002_S.tir');
      const model = loadModel(mf, text, 'pac2002_S.tir');
      const converted = mf.convertModel(text, 'pac2002_S.tir', 'ISO');
      expect(converted.ok).toBe(true);
      if (!converted.ok) return;
      expect(converted.applied).toEqual({ vertical: false, lateral: true });
      const negated = new Set(['DFY_DSA', 'FY_OVER_SA', 'SA_AT_PEAK_FY']);
      for (const alpha of [0.05, -0.05]) {
        const iso = characteristicsOf(mf, converted.text, model, [at(4000, alpha, 0.1)]);
        const mixed = characteristicsOf(mf, text, model, [at(4000, -alpha, 0.1)]);
        const a = row(0, 1, iso.characteristics);
        const b = row(0, 1, mixed.characteristics);
        NAMES.forEach((name, c) => near(a[c]!, (negated.has(name) ? -1 : 1) * b[c]!, name, `lateral alpha ${alpha}`));
      }
    });

    it('negates the load-dependent characteristics for the vertical conversion', async () => {
      const mf = await createMfModule();
      const isoText = readData('pac2002_S_iso.tir');
      const model = loadModel(mf, isoText, 'pac2002_S_iso.tir');
      const converted = mf.convertModel(isoText, 'pac2002_S_iso.tir', 'SAE');
      expect(converted.ok).toBe(true);
      if (!converted.ok) return;
      expect(converted.applied).toEqual({ vertical: true, lateral: false });
      const negated = new Set(['DFY_DSA', 'FY_OVER_SA', 'DY']);
      const sae = characteristicsOf(mf, converted.text, model, [at(-4000, 0.05, 0.1)]);
      const iso = characteristicsOf(mf, isoText, model, [at(4000, 0.05, 0.1)]);
      const a = row(0, 1, sae.characteristics);
      const b = row(0, 1, iso.characteristics);
      NAMES.forEach((name, c) => near(a[c]!, (negated.has(name) ? -1 : 1) * b[c]!, name, 'vertical'));
    });

    it('rejects an input of the wrong shape', async () => {
      const mf = await createMfModule();
      const result = mf.evaluateWithCharacteristics(readData('pac2002_S.tir'), new Float64Array(7), false);
      expect(result.ok).toBe(false);
      if (result.ok) return;
      expect(result.error.code).toBe('INVALID_ARGUMENT');
      expect(result.row).toBeNull();
    });
  });

  describe('coordinate systems', () => {
    const batchFor = (model: RawModel, rows: Record<string, number>[]): Float64Array => {
      const data = new Float64Array(rows.length * model.inputs.length);
      model.inputs.forEach((channel, k) => {
        rows.forEach((row, r) => {
          data[k * rows.length + r] = row[channel.name] ?? 0;
        });
      });
      return data;
    };
    const isoRows = [0.02, 0.05, 0.1].map((SA) => ({ SA, FZ: 4000, VX: 16.6 }));
    const saeRows = isoRows.map((row) => ({ ...row, FZ: -row.FZ }));

    const classify = (mf: MfWasmModule, text: string, name: string) => {
      const inspection = mf.inspect(text, name);
      expect(inspection.ok).toBe(true);
      if (!inspection.ok) throw new Error('inspect failed');
      return inspection.coordinates;
    };

    it('identifies the repo model as mixed, with every indicator reading', async () => {
      const mf = await createMfModule();
      const id = classify(mf, readData('pac2002_S.tir'), 'pac2002_S.tir');
      expect(id?.classification).toBe('MIXED');
      expect(id?.readings).toHaveLength(29);
    });

    it('identifies the ISO and SAE fixtures', async () => {
      const mf = await createMfModule();
      expect(classify(mf, readData('pac2002_S_iso.tir'), 'pac2002_S_iso.tir')?.classification).toBe('ISO');
      expect(classify(mf, readData('pac2002_S_sae.tir'), 'pac2002_S_sae.tir')?.classification).toBe('SAE');
    });

    it('converts an ISO model to SAE by coefficient sign flips', async () => {
      const mf = await createMfModule();
      const converted = mf.convertModel(readData('pac2002_S_iso.tir'), 'pac2002_S_iso.tir', 'SAE');
      expect(converted.ok).toBe(true);
      if (!converted.ok) return;
      expect(converted.text).toMatch(/FNOMIN\s*=\s*-4000/);
      expect(converted.applied.vertical).toBe(true);
      expect(classify(mf, converted.text, 'converted.tir')?.classification).toBe('SAE');
    });

    it('evaluates the SAE model on FZ-negated rows as the sign-flipped ISO result', async () => {
      const mf = await createMfModule();
      const isoText = readData('pac2002_S_iso.tir');
      const saeText = readData('pac2002_S_sae.tir');
      const model = loadModel(mf, isoText, 'pac2002_S_iso.tir');
      const iso = mf.evaluate(isoText, batchFor(model, isoRows), false);
      const sae = mf.evaluate(saeText, batchFor(model, saeRows), false);
      expect(iso.ok && sae.ok).toBe(true);
      if (!iso.ok || !sae.ok) return;
      const rows = isoRows.length;
      const fy = model.outputs.findIndex((c) => c.name === 'FY');
      for (let r = 0; r < rows; r += 1) {
        const a = sae.outputs[fy * rows + r]!;
        const b = iso.outputs[fy * rows + r]!;
        expect(Math.abs(a - -b)).toBeLessThanOrEqual(1e-12 * (4000 + Math.abs(b)));
      }
    });

    it('warns once about FZ < 0 rows of an ISO table', async () => {
      const mf = await createMfModule();
      const text = readData('pac2002_S_iso.tir');
      const model = loadModel(mf, text, 'pac2002_S_iso.tir');
      const rows = [...isoRows];
      rows[1] = { ...rows[1]!, FZ: -4000 };
      const result = mf.evaluate(text, batchFor(model, rows), false);
      expect(result.ok).toBe(true);
      if (!result.ok) return;
      expect(result.warnings).toHaveLength(1);
      expect(result.warnings[0]!.message.startsWith('FZ < 0 in 1 row(s) of an ISO table')).toBe(true);
      expect(result.warnings[0]!.row).toBe(1);
    });

    it('rejects an unknown target system', async () => {
      const mf = await createMfModule();
      const result = mf.convertModel(readData('pac2002_S_iso.tir'), 'x.tir', 'NED' as never);
      expect(result.ok).toBe(false);
      if (result.ok) return;
      expect(result.error.code).toBe('INVALID_ARGUMENT');
    });
  });
});

describe('MF6.1 full model', () => {
  it('loads pressure and turn-slip channels and evaluates radii and characteristics', async () => {
    const mf = await createMfModule();
    const text = readData('mf61_solver.tir');
    const inspected = mf.inspect(text, 'mf61_solver.tir');
    expect(inspected.ok).toBe(true);
    if (!inspected.ok) throw new Error(inspected.error.message);
    expect(inspected.model?.name).toBe('MF 6.1');
    expect(inspected.model?.inputs.map((channel) => channel.name)).toEqual(['FZ', 'SA', 'SR', 'IA', 'VX', 'P', 'TS']);
    const point = new Float64Array([5000, 0.02, 0.1, 0.01, 27, 180000, 0.05]);
    const disabled = mf.evaluate(text, point, false);
    expect(disabled.ok).toBe(true);
    if (!disabled.ok) throw new Error(disabled.error.message);
    expect(disabled.warnings.some((item) => item.key === 'TS')).toBe(true);
    const enabled = mf.evaluateWithCharacteristicsTurnSlip(text, point, false, true);
    expect(enabled.ok).toBe(true);
    if (!enabled.ok) throw new Error(enabled.error.message);
    expect(enabled.outputs).toHaveLength(13);
    expect(enabled.characteristics).toHaveLength(8);
    expect(enabled.outputs.every(Number.isFinite)).toBe(true);
    expect(enabled.outputs[11]).toBeGreaterThan(0);
    expect(enabled.outputs[12]).toBeGreaterThan(0);
    expect(enabled.warnings.some((item) => item.key === 'TS')).toBe(false);
  });
});
