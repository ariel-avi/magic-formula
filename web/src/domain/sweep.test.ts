import { describe, expect, it } from 'vitest';
import { linspace, parseValueList } from './sweep';

describe('linspace', () => {
  it('includes both endpoints with a uniform step', () => {
    const values = linspace(0, 1, 5);
    [0, 0.25, 0.5, 0.75, 1].forEach((expected, i) => expect(values[i]).toBeCloseTo(expected, 12));
    expect(values).toHaveLength(5);
  });

  it('supports descending ranges', () => {
    expect(linspace(1, -1, 3)).toEqual([1, 0, -1]);
  });

  it('returns start and end for two points', () => {
    expect(linspace(2, 7, 2)).toEqual([2, 7]);
  });

  it('ends exactly on the end value', () => {
    expect(linspace(0, 0.3, 7)[6]).toBe(0.3);
  });

  it.each([1, 2.5, 0])('throws RangeError for %s points', (points) => {
    expect(() => linspace(0, 1, points)).toThrow(RangeError);
  });
});

describe('parseValueList', () => {
  it('splits on commas, blanks and tabs', () => {
    expect(parseValueList('1, 2 3\t4')).toEqual({ ok: true, value: [1, 2, 3, 4] });
  });

  it('accepts exponents and signs', () => {
    expect(parseValueList('1e3,-2.5E-1')).toEqual({ ok: true, value: [1000, -0.25] });
  });

  it.each(['', '  '])('rejects an empty list %j', (text) => {
    expect(parseValueList(text)).toEqual({ ok: false, error: 'Enter at least one value.' });
  });

  it('rejects an empty entry', () => {
    expect(parseValueList('1,,2')).toEqual({ ok: false, error: 'Empty value in the list.' });
  });

  it.each(['abc', 'NaN', 'Infinity', '1e999'])('rejects %s', (token) => {
    expect(parseValueList(`1 ${token}`)).toEqual({ ok: false, error: `'${token}' is not a finite number.` });
  });
});
