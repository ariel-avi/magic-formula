import { describe, expect, it, vi } from 'vitest';
import { buildBatch } from './batch';
import type { SweepSpec } from './chart-spec';
import type { InputChannel } from './model';
import { err, ok } from './result';
import { solveSweep, solveSweeps, solvedSweeps, type BatchSolver, type SweepOutcome } from './solve';

const channel = (name: string): InputChannel => ({ name, unit: '-', minimum: 0, minimumInclusive: true, negatedBy: name === 'FZ' ? 'VERTICAL' : name === 'SA' ? 'LATERAL' : 'NONE' });
const inputs = [channel('FZ'), channel('SA')];

const sweep: SweepSpec = {
  id: 'sweep-1',
  name: 'Sweep 1',
  swept: { variable: 'SA', start: 0, end: 10, points: 3 },
  steps: [{ variable: 'FZ', values: [5000], colourMode: 'hue' }],
  fixed: {},
  uncombined: false,
};

const failure = (row: number | null) =>
  err({ code: 'NUMERICAL' as const, message: 'boom', line: 0, key: '', row });

describe('solveSweep', () => {
  it('reports problems without calling the solver', () => {
    const solve = vi.fn<BatchSolver>();
    const outcome = solveSweep({ ...sweep, swept: { ...sweep.swept, points: 1 } }, inputs, solve);
    expect(solve).not.toHaveBeenCalled();
    expect(outcome.problems.length).toBeGreaterThan(0);
    expect(outcome.warnings).toEqual([]);
    expect(outcome.error).toBeNull();
    expect(outcome.solution).toBeNull();
  });

  it('appends the failing row location to the solver error', () => {
    const solve = vi.fn<BatchSolver>(() => failure(0));
    expect(solveSweep(sweep, inputs, solve)).toEqual({
      problems: [],
      error: 'boom (FZ = 5000, SA = 0)',
      solution: null,
      warnings: [],
    });
  });

  it('reports the message alone when the row is null', () => {
    const outcome = solveSweep(sweep, inputs, () => failure(null));
    expect(outcome.error).toBe('boom');
  });

  it('solves the batch with the sweep slip mode', () => {
    const outputs = new Float64Array([1, 2, 3]);
    const solve = vi.fn<BatchSolver>(() => ok({ outputs, warnings: [] }));
    const pure = { ...sweep, uncombined: true };
    const outcome = solveSweep(pure, inputs, solve);
    expect(solve).toHaveBeenCalledTimes(1);
    expect(solve).toHaveBeenCalledWith(buildBatch(pure, inputs), true);
    expect(outcome.problems).toEqual([]);
    expect(outcome.error).toBeNull();
    expect(outcome.solution?.outputs).toBe(outputs);
    expect(outcome.solution?.batch).toEqual(buildBatch(pure, inputs));
    expect(outcome.warnings).toEqual([]);
  });

  it('formats the solver warnings in order', () => {
    const outputs = new Float64Array(3);
    const warnings = [
      { message: 'first', key: 'FZ', row: 0 },
      { message: 'second', key: 'FZ', row: 2 },
    ];
    const outcome = solveSweep(sweep, inputs, () => ok({ outputs, warnings }));
    expect(outcome.warnings).toEqual(['first (first at FZ = 5000, SA = 0)', 'second (first at FZ = 5000, SA = 10)']);
    expect(outcome.solution?.outputs).toBe(outputs);
  });
});

describe('solveSweeps', () => {
  const okSolver = () => vi.fn<BatchSolver>(() => ok({ outputs: new Float64Array(3), warnings: [] }));

  it('solves every sweep once and reuses the cached outcomes', () => {
    const sweeps = [sweep, { ...sweep, id: 'sweep-2' }];
    const cache = new WeakMap<SweepSpec, SweepOutcome>();
    const solve = okSolver();
    const first = solveSweeps(sweeps, inputs, cache, solve);
    expect(solve).toHaveBeenCalledTimes(2);
    const again = solveSweeps(sweeps, inputs, cache, solve);
    expect(solve).toHaveBeenCalledTimes(2);
    expect(again[0]).toBe(first[0]);
    expect(again[1]).toBe(first[1]);
  });

  it('solves only the replaced sweep', () => {
    const sweeps = [sweep, { ...sweep, id: 'sweep-2' }];
    const cache = new WeakMap<SweepSpec, SweepOutcome>();
    const solve = okSolver();
    const first = solveSweeps(sweeps, inputs, cache, solve);
    const edited = [sweeps[0], { ...sweeps[1], uncombined: true }];
    const next = solveSweeps(edited, inputs, cache, solve);
    expect(solve).toHaveBeenCalledTimes(3);
    expect(next[0]).toBe(first[0]);
    expect(next[1]).not.toBe(first[1]);
  });
});

describe('solvedSweeps', () => {
  it('keeps the solved sweeps with their chart position', () => {
    const sweeps = [sweep, { ...sweep, id: 'sweep-2' }, { ...sweep, id: 'sweep-3' }];
    const solution = { batch: buildBatch(sweep, inputs), outputs: new Float64Array(3) };
    const solved: SweepOutcome = { problems: [], error: null, solution, warnings: [] };
    const unsolved: SweepOutcome = { problems: [], error: 'x', solution: null, warnings: [] };
    const result = solvedSweeps(sweeps, [solved, unsolved, solved]);
    expect(result.map((r) => r.index)).toEqual([0, 2]);
    expect(result[0].sweep).toBe(sweeps[0]);
    expect(result[1].sweep).toBe(sweeps[2]);
    expect(result[0].solution).toBe(solution);
  });
});
