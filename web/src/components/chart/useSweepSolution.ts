import { useMemo } from 'react';
import { buildBatch, describeBatchRow, type SweepBatch } from '@/domain/batch';
import type { ChartSpec } from '@/domain/chart-spec';
import type { ModelInfo } from '@/domain/model';
import { validateChartSpec } from '@/domain/steps';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { solveBatch } from '@/wasm/solver';

export interface SweepSolution {
  batch: SweepBatch;
  outputs: Float64Array;
}

interface Outcome {
  error: string | null;
  solution: SweepSolution | null;
}

/** Validates the spec and solves the sweep; the theme is not an input, so a theme change never re-solves. */
export function useSweepSolution(
  mf: MfWasmModule,
  text: string,
  model: ModelInfo,
  spec: ChartSpec,
): { problems: string[]; error: string | null; solution: SweepSolution | null } {
  const problems = useMemo(() => validateChartSpec(spec, model.inputs, model.outputs), [spec, model]);
  const outcome = useMemo((): Outcome => {
    if (problems.length > 0) {
      return { error: null, solution: null };
    }
    const batch = buildBatch(spec, model.inputs);
    const result = solveBatch(mf, text, batch, spec.uncombined);
    if (!result.ok) {
      const row = result.error.row;
      const where = row === null || row === undefined ? '' : ` (${describeBatchRow(spec, batch, row)})`;
      return { error: `${result.error.message}${where}`, solution: null };
    }
    return { error: null, solution: { batch, outputs: result.value } };
  }, [mf, text, model, spec, problems]);
  return { problems, ...outcome };
}
