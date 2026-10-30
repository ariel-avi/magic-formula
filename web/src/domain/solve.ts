import { buildBatch, describeBatchRow, type SweepBatch } from './batch';
import { formatBatchWarning } from './coordinates';
import type { SweepSpec } from './chart-spec';
import type { EvaluationError, InputChannel, SolvedBatch } from './model';
import type { Result } from './result';
import { validateSweepSpec } from './steps';

export interface SweepSolution {
  batch: SweepBatch;
  outputs: Float64Array;
  characteristics: Float64Array | null;
}

export interface SweepOutcome {
  problems: string[];
  error: string | null;
  solution: SweepSolution | null;
  /** Formatted solver warnings; empty for validation problems and solver errors. */
  warnings: string[];
}

export type BatchSolver = (
  batch: SweepBatch,
  uncombined: boolean,
  characteristics: boolean,
  turnSlip?: boolean,
) => Result<SolvedBatch, EvaluationError>;

/** Validates and solves one sweep. Invalid specs never reach the solver; a solver error names the failing row. */
export function solveSweep(
  sweep: SweepSpec,
  inputs: readonly InputChannel[],
  solve: BatchSolver,
  characteristics: boolean,
): SweepOutcome {
  const problems = validateSweepSpec(sweep, inputs);
  if (problems.length > 0) {
    return { problems, error: null, solution: null, warnings: [] };
  }
  const batch = buildBatch(sweep, inputs);
  const result = sweep.turnSlip
    ? solve(batch, sweep.uncombined, characteristics, true)
    : solve(batch, sweep.uncombined, characteristics);
  if (!result.ok) {
    const row = result.error.row;
    const error = result.error.message + (typeof row === 'number' ? ` (${describeBatchRow(sweep, batch, row)})` : '');
    return { problems: [], error, solution: null, warnings: [] };
  }
  const warnings = result.value.warnings.map((warning) => formatBatchWarning(sweep, batch, warning));
  return { problems: [], error: null, solution: { batch, outputs: result.value.outputs, characteristics: result.value.characteristics }, warnings };
}

export interface SolveCaches {
  plain: WeakMap<SweepSpec, SweepOutcome>;
  withCharacteristics: WeakMap<SweepSpec, SweepOutcome>;
}

export function createSolveCaches(): SolveCaches {
  return { plain: new WeakMap(), withCharacteristics: new WeakMap() };
}

/**
 * One outcome per sweep; a sweep object already in the cache of the mode is not solved again.
 * Characteristics are solved only on demand: there is one cache per mode, so switching between a
 * characteristic axis and a plain axis re-solves only the missing mode.
 */
export function solveSweeps(
  sweeps: readonly SweepSpec[],
  inputs: readonly InputChannel[],
  caches: SolveCaches,
  solve: BatchSolver,
  characteristics: boolean,
): SweepOutcome[] {
  const cache = characteristics ? caches.withCharacteristics : caches.plain;
  return sweeps.map((sweep) => {
    const hit = cache.get(sweep);
    if (hit !== undefined) {
      return hit;
    }
    const outcome = solveSweep(sweep, inputs, solve, characteristics);
    cache.set(sweep, outcome);
    return outcome;
  });
}

export interface SolvedSweep {
  /** Position in the chart: drives the dash, the base colour slot and the default name. */
  index: number;
  sweep: SweepSpec;
  solution: SweepSolution;
}

export function solvedSweeps(sweeps: readonly SweepSpec[], outcomes: readonly SweepOutcome[]): SolvedSweep[] {
  return sweeps.flatMap((sweep, index) => {
    const solution = outcomes[index].solution;
    return solution === null ? [] : [{ index, sweep, solution }];
  });
}
