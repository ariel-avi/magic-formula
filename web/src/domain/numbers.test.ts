import { describe, expect, it } from 'vitest';
import { formatLimit, formatNumber, formatSignificant, parseNumber } from './numbers';

describe('parseNumber', () => {
  it.each([
    ['1', 1],
    ['-2.5', -2.5],
    ['+.5', 0.5],
    ['1.', 1],
    ['-.5', -0.5],
    ['2.1e+005', 210000],
    ['0e5', 0],
    [' 3 ', 3],
  ])('accepts %j', (text, expected) => {
    expect(parseNumber(text)).toEqual({ ok: true, value: expected });
  });

  it('keeps the sign of negative zero', () => {
    const result = parseNumber('-0.0');
    expect(result.ok && Object.is(result.value, -0)).toBe(true);
  });

  it.each(['', '   '])('rejects blank text %j', (text) => {
    expect(parseNumber(text)).toEqual({ ok: false, error: 'Enter a number.' });
  });

  it.each(['abc', '1,5', '+-1', '++1', '1e', '.', 'inf', 'NaN', 'Infinity', '0x10', '1.0D-3'])(
    'rejects %j',
    (text) => {
      expect(parseNumber(text)).toEqual({ ok: false, error: `'${text}' is not a finite number.` });
    },
  );

  it('rejects overflow', () => {
    expect(parseNumber('1e999')).toEqual({ ok: false, error: "'1e999' is not a finite number." });
  });

  it('rejects underflow to zero', () => {
    expect(parseNumber('1e-400')).toEqual({ ok: false, error: "'1e-400' is not a finite number." });
  });
});

describe('formatNumber', () => {
  it('prints the shortest round-trip text', () => {
    expect(formatNumber(4000)).toBe('4000');
    expect(formatNumber(0.1)).toBe('0.1');
  });

  it('prints nothing for non-finite values', () => {
    expect(formatNumber(NaN)).toBe('');
    expect(formatNumber(Infinity)).toBe('');
  });
});

describe('formatSignificant', () => {
  it('rounds to significant digits', () => {
    expect(formatSignificant(1234.5678, 4)).toBe('1235');
    expect(formatSignificant(0.000123456, 2)).toBe('0.00012');
  });

  it('prints nothing for non-finite values', () => {
    expect(formatSignificant(NaN, 4)).toBe('');
    expect(formatSignificant(-Infinity, 4)).toBe('');
  });
});

describe('formatLimit', () => {
  it('prints finite limits exactly', () => {
    expect(formatLimit(2500)).toBe('2500');
  });

  it('prints an absent bound as infinity and NaN as a dash', () => {
    expect(formatLimit(Number.POSITIVE_INFINITY)).toBe('+∞');
    expect(formatLimit(Number.NEGATIVE_INFINITY)).toBe('−∞');
    expect(formatLimit(Number.NaN)).toBe('—');
  });
});
