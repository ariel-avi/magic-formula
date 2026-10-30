import type { SweepBatch } from '@/domain/batch';
import type {
  Conversion,
  CoordinateSystem,
  EvaluationError,
  MfError,
  SolvedBatch,
  TirInspection,
} from '@/domain/model';
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
    coordinates: raw.coordinates,
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

/**
 * Solves the batch. The outputs and the characteristics (when requested) are column-major, rows x model channels;
 * without them `characteristics` is null.
 */
export function solveBatch(
  mf: MfWasmModule,
  text: string,
  batch: SweepBatch,
  uncombined: boolean,
  characteristics: boolean,
  turnSlip = false,
): Result<SolvedBatch, EvaluationError> {
  if (characteristics) {
    const raw = turnSlip
      ? mf.evaluateWithCharacteristicsTurnSlip(text, batch.inputs, uncombined, true)
      : mf.evaluateWithCharacteristics(text, batch.inputs, uncombined);
    return raw.ok
      ? ok({ outputs: raw.outputs, characteristics: raw.characteristics, warnings: raw.warnings })
      : err({ ...raw.error, row: raw.row });
  }
  const raw = turnSlip
    ? mf.evaluateTurnSlip(text, batch.inputs, uncombined, true)
    : mf.evaluate(text, batch.inputs, uncombined);
  return raw.ok
    ? ok({ outputs: raw.outputs, characteristics: null, warnings: raw.warnings })
    : err({ ...raw.error, row: raw.row });
}

/** Converts the model to the target system by coefficient sign flips; returns the converted .tir text. */
export function convertModel(
  mf: MfWasmModule,
  text: string,
  fileName: string,
  target: CoordinateSystem,
): Result<Conversion, MfError> {
  const raw = mf.convertModel(text, fileName, target);
  return raw.ok ? ok({ text: raw.text, applied: raw.applied, changes: raw.changes }) : err(raw.error);
}
