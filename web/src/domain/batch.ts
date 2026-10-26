import type { SweepSpec } from './chart-spec';
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
export function buildBatch(spec: SweepSpec, inputs: readonly InputChannel[]): SweepBatch {
  const { points } = spec.swept;
  const grid = linspace(spec.swept.start, spec.swept.end, points);
  const combinations = stepCombinations(spec.steps);
  const rows = combinations.length * points;
  const data = new Float64Array(rows * inputs.length);

  inputs.forEach((channel, k) => {
    const unit = displayUnit(channel.unit);
    const stepIndex = spec.steps.findIndex((step) => step.variable === channel.name);
    const swept = channel.name === spec.swept.variable;
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

/** Location of a failing solver row, e.g. 'FZ = 2000, SA = -15': the step values of the row's combination, then the swept value. */
export function describeBatchRow(spec: SweepSpec, batch: SweepBatch, row: number): string {
  const { points } = spec.swept;
  const combination = batch.combinations[Math.floor(row / points)];
  const parts = spec.steps.map((step, k) => `${step.variable} = ${combination.values[k]}`);
  parts.push(`${spec.swept.variable} = ${batch.grid[row % points]}`);
  return parts.join(', ');
}
