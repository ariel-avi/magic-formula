import { describe, expect, it } from 'vitest';
import { MAX_STEP_VALUES } from './chart-spec';
import { addStepValues, removeStepValue, stepRange } from './step-values';

describe('addStepValues', () => {
  it('adds a single value', () => {
    expect(addStepValues([1000], '2000')).toEqual({ ok: true, value: [1000, 2000] });
  });

  it('adds a list and sorts the result', () => {
    expect(addStepValues([], '3, 1 2')).toEqual({ ok: true, value: [1, 2, 3] });
  });

  it('rejects a duplicate of an existing value', () => {
    expect(addStepValues([1000], '1000')).toEqual({ ok: false, error: '1000 is already in the list.' });
  });

  it('rejects a duplicate within the text', () => {
    expect(addStepValues([], '5 5')).toEqual({ ok: false, error: '5 is already in the list.' });
  });

  it('passes parse errors through', () => {
    expect(addStepValues([], 'abc')).toEqual({ ok: false, error: "'abc' is not a finite number." });
    expect(addStepValues([], ' ')).toEqual({ ok: false, error: 'Enter at least one value.' });
  });

  it('rejects more values than a step takes', () => {
    const full = Array.from({ length: MAX_STEP_VALUES }, (_, i) => i);
    expect(addStepValues(full, '100')).toEqual({
      ok: false,
      error: `A step takes at most ${MAX_STEP_VALUES} values.`,
    });
  });

  it('does not modify its input', () => {
    const values = [2, 1];
    addStepValues(values, '3');
    expect(values).toEqual([2, 1]);
  });
});

describe('removeStepValue', () => {
  it('removes the value at the index', () => {
    expect(removeStepValue([1, 2, 3], 1)).toEqual([1, 3]);
  });

  it('keeps the last remaining value', () => {
    const values = [7];
    const result = removeStepValue(values, 0);
    expect(result).toEqual([7]);
    expect(result).not.toBe(values);
  });
});

describe('stepRange', () => {
  it('gives clean decimals', () => {
    expect(stepRange(-0.3, 0.3, 7)).toEqual({ ok: true, value: [-0.3, -0.2, -0.1, 0, 0.1, 0.2, 0.3] });
  });

  it('returns ascending values for a descending range', () => {
    expect(stepRange(0.3, -0.3, 7)).toEqual({ ok: true, value: [-0.3, -0.2, -0.1, 0, 0.1, 0.2, 0.3] });
  });

  it('spaces integers evenly', () => {
    expect(stepRange(1000, 5000, 5)).toEqual({ ok: true, value: [1000, 2000, 3000, 4000, 5000] });
  });

  it.each([1, MAX_STEP_VALUES + 1, 2.5])('rejects count %j', (count) => {
    expect(stepRange(0, 1, count)).toEqual({
      ok: false,
      error: `Count must be an integer from 2 to ${MAX_STEP_VALUES}.`,
    });
  });

  it.each([
    [1, 1, 3],
    [NaN, 1, 3],
    [0, Infinity, 3],
  ])('rejects the range %j to %j', (start, end, count) => {
    expect(stepRange(start, end, count)).toEqual({ ok: false, error: 'From and to must be finite and different.' });
  });
});
