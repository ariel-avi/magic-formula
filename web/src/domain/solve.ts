import { buildBatch, describeBatchRow, type SweepBatch } from './batch';
import type { SweepSpec } from './chart-spec';
import type { EvaluationError, InputChannel } from './model';
import type { Result } from './result';
import { validateSweepSpec } from './steps';

export interface SweepSolution {
  batch: SweepBatch;
  outputs: Float64Array;
}

export interface SweepOutcome {
  problems: string[];
  error: string | null;
  solution: SweepSolution | null;
}

export type BatchSolver = (batch: SweepBatch, uncombined: boolean) => Result<Float64Array, EvaluationError>;

/** Validates and solves one sweep. Invalid specs never reach the solver; a solver error names the failing row. */
export function solveSweep(sweep: SweepSpec, inputs: readonly InputChannel[], solve: BatchSolver): SweepOutcome {
  const problems = validateSweepSpec(sweep, inputs);
  if (problems.length > 0) {
    return { problems, error: null, solution: null };
  }
  const batch = buildBatch(sweep, inputs);
  const result = solve(batch, sweep.uncombined);
  if (!result.ok) {
    const row = result.error.row;
    const error = result.error.message + (typeof row === 'number' ? ` (${describeBatchRow(sweep, batch, row)})` : '');
    return { problems: [], error, solution: null };
  }
  return { problems: [], error: null, solution: { batch, outputs: result.value } };
}

/** One outcome per sweep; a sweep object already in the cache is not solved again. */
export function solveSweeps(
  sweeps: readonly SweepSpec[],
  inputs: readonly InputChannel[],
  cache: WeakMap<SweepSpec, SweepOutcome>,
  solve: BatchSolver,
): SweepOutcome[] {
  return sweeps.map((sweep) => {
    const hit = cache.get(sweep);
    if (hit !== undefined) {
      return hit;
    }
    const outcome = solveSweep(sweep, inputs, solve);
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
