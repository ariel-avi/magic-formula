import { useMemo } from 'react';
import type { SweepSpec } from '@/domain/chart-spec';
import type { ModelInfo } from '@/domain/model';
import { solveSweeps, type BatchSolver, type SweepOutcome } from '@/domain/solve';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { solveBatch } from '@/wasm/solver';

/** One outcome per sweep; a sweep object that survives an edit of another sweep is a cache hit, and the theme is not an input. */
export function useSweepSolutions(
  mf: MfWasmModule,
  text: string,
  model: ModelInfo,
  sweeps: readonly SweepSpec[],
): SweepOutcome[] {
  const cache = useMemo(() => new WeakMap<SweepSpec, SweepOutcome>(), [mf, text, model]);
  return useMemo(() => {
    const solve: BatchSolver = (batch, uncombined) => solveBatch(mf, text, batch, uncombined);
    return solveSweeps(sweeps, model.inputs, cache, solve);
  }, [cache, mf, text, model, sweeps]);
}
