import { describe, expect, it } from 'vitest';
import { niceExtent, niceTicks } from './ticks';

describe('niceTicks', () => {
  it('uses round steps and includes zero for a symmetric range', () => {
    expect(niceTicks(-15, 15, 7)).toEqual([-15, -10, -5, 0, 5, 10, 15]);
  });

  it('covers every step size class', () => {
    expect(niceTicks(0, 10, 10)).toEqual([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    expect(niceTicks(0, 20, 10)).toEqual([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20]);
    expect(niceTicks(0, 50, 10)).toEqual([0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50]);
    expect(niceTicks(0, 80, 10)).toEqual([0, 10, 20, 30, 40, 50, 60, 70, 80]);
  });

  it('keeps ticks inside a range that does not start on a step', () => {
    expect(niceTicks(800, 8000, 8)).toEqual([1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000]);
  });

  it('removes floating-point noise', () => {
    expect(niceTicks(0, 0.3, 3)).toEqual([0, 0.1, 0.2, 0.3]);
  });

  it('returns the single value of a degenerate range and nothing for non-finite input', () => {
    expect(niceTicks(2, 2, 5)).toEqual([2]);
    expect(niceTicks(Number.NaN, 2, 5)).toEqual([]);
    expect(niceTicks(0, Number.POSITIVE_INFINITY, 5)).toEqual([]);
  });
});

describe('niceExtent', () => {
  it('rounds outward to the tick step', () => {
    expect(niceExtent(-3.2, 7.9, 5)).toEqual([-5, 10]);
    expect(niceExtent(-5900, 5900, 8)).toEqual([-6000, 6000]);
    expect(niceExtent(0, 5, 5)).toEqual([0, 5]);
    expect(niceExtent(0, 12, 5)).toEqual([0, 12.5]);
    expect(niceExtent(0, 30, 5)).toEqual([0, 30]);
    expect(niceExtent(0, 10, 5)).toEqual([0, 10]);
  });

  it('widens a degenerate range', () => {
    expect(niceExtent(0, 0, 5)).toEqual([-1, 1]);
  });
});
