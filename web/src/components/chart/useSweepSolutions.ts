import { useMemo } from 'react';
import type { SweepSpec } from '@/domain/chart-spec';
import type { ModelInfo } from '@/domain/model';
import { createSolveCaches, solveSweeps, type BatchSolver, type SweepOutcome } from '@/domain/solve';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { solveBatch } from '@/wasm/solver';

/** One outcome per sweep; a sweep object that survives an edit of another sweep is a cache hit, and the theme is not an input. Characteristics are solved only when asked for. */
export function useSweepSolutions(
  mf: MfWasmModule,
  text: string,
  model: ModelInfo,
  sweeps: readonly SweepSpec[],
  characteristics: boolean,
): SweepOutcome[] {
  const caches = useMemo(createSolveCaches, [mf, text, model]);
  return useMemo(() => {
    const solve: BatchSolver = (batch, uncombined, withCharacteristics) =>
      solveBatch(mf, text, batch, uncombined, withCharacteristics);
    return solveSweeps(sweeps, model.inputs, caches, solve, characteristics);
  }, [caches, mf, text, model, sweeps, characteristics]);
}
