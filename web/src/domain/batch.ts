import type { ChartSpec } from './chart-spec';
import type { InputChannel } from './model';
import { stepCombinations, type Combination } from './steps';
import { linspace } from './sweep';
import { displayUnit } from './units';

export interface SweepBatch {
  /** Column-major, rows x inputs.length, SI units. */
  inputs: Float64Array;
  rows: number;
  /** Swept values in display units. */
  grid: number[];
  combinations: Combination[];
}

/**
 * Builds the solver input batch for a validated spec. Row r = combination * points + point;
 * column k is inputs[k] in layout order; element (r, k) sits at k * rows + r.
 */
export function buildBatch(spec: ChartSpec, inputs: readonly InputChannel[]): SweepBatch {
  const { points } = spec.sweep;
  const grid = linspace(spec.sweep.start, spec.sweep.end, points);
  const combinations = stepCombinations(spec.steps);
  const rows = combinations.length * points;
  const data = new Float64Array(rows * inputs.length);

  inputs.forEach((channel, k) => {
    const unit = displayUnit(channel.unit);
    const stepIndex = spec.steps.findIndex((step) => step.variable === channel.name);
    const swept = channel.name === spec.sweep.variable;
    combinations.forEach((combination, c) => {
      for (let i = 0; i < points; i++) {
        let value = spec.fixed[channel.name];
        if (swept) {
          value = grid[i];
        } else if (stepIndex >= 0) {
          value = combination.values[stepIndex];
        }
        data[k * rows + c * points + i] = unit.toSi(value);
      }
    });
  });

  return { inputs: data, rows, grid, combinations };
}
