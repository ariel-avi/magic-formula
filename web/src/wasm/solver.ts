import type { SweepBatch } from '@/domain/batch';
import type { EvaluationError, MfError, TirInspection } from '@/domain/model';
import { err, ok, type Result } from '@/domain/result';
import createMfModule, { type MfWasmModule } from './mf_wasm.js';

let modulePromise: Promise<MfWasmModule> | undefined;

/** Loads the WebAssembly module once. */
export function loadSolver(): Promise<MfWasmModule> {
  modulePromise ??= createMfModule();
  return modulePromise;
}

/** Parses the text and builds the model; a syntax error is an Err, a model load error stays in the inspection. */
export function inspectTir(mf: MfWasmModule, text: string, fileName: string): Result<TirInspection, MfError> {
  const raw = mf.inspect(text, fileName);
  if (!raw.ok) {
    return err(raw.error);
  }
  // The solver reports unconsumed keys without sections, so a key that appears in more than one
  // section is never marked unused, and nothing is marked unused when no model loaded.
  const ignored = new Set(raw.model === null ? [] : raw.ignoredKeys);
  const keyCounts = new Map<string, number>();
  for (const entry of raw.entries) {
    keyCounts.set(entry.key, (keyCounts.get(entry.key) ?? 0) + 1);
  }
  return ok({
    entries: raw.entries.map((entry) => ({
      ...entry,
      used: !ignored.has(entry.key) || (keyCounts.get(entry.key) ?? 0) > 1,
    })),
    warnings: raw.warnings,
    ignoredKeys: raw.ignoredKeys,
    model: raw.model,
    loadError: raw.loadError,
  });
}

/** Returns the text with one entry replaced. */
export function setEntry(
  mf: MfWasmModule,
  text: string,
  section: string,
  key: string,
  value: string,
): Result<string, MfError> {
  const raw = mf.replaceValue(text, section, key, value);
  return raw.ok ? ok(raw.text) : err(raw.error);
}

/** Solves the batch; the outputs are column-major, rows x model outputs. */
export function solveBatch(
  mf: MfWasmModule,
  text: string,
  batch: SweepBatch,
  uncombined: boolean,
): Result<Float64Array, EvaluationError> {
  const raw = mf.evaluate(text, batch.inputs, uncombined);
  return raw.ok ? ok(raw.outputs) : err({ ...raw.error, row: raw.row });
}
