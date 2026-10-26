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
