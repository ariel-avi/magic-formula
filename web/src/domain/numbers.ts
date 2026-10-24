import { err, ok, type Result } from './result';

/** The solver's number grammar: optional sign, digits with an optional fraction (or a leading-dot fraction), optional exponent. */
export const NUMBER_PATTERN = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;

/** One finite number; surrounding blanks are ignored. Rejects what the solver rejects, including overflow and underflow to zero. */
export function parseNumber(text: string): Result<number, string> {
  const trimmed = text.trim();
  if (trimmed === '') {
    return err('Enter a number.');
  }
  const failure = err(`'${trimmed}' is not a finite number.`);
  if (!NUMBER_PATTERN.test(trimmed)) {
    return failure;
  }
  const value = Number(trimmed);
  if (!Number.isFinite(value)) {
    return failure;
  }
  if (value === 0 && /[1-9]/.test(trimmed.split(/[eE]/)[0])) {
    return failure;
  }
  return ok(value);
}

/** Shortest text that reads back as `value`; '' for NaN and infinities. */
export function formatNumber(value: number): string {
  return Number.isFinite(value) ? String(value) : '';
}

/** `value` rounded to `digits` significant digits as short text; '' for non-finite values. */
export function formatSignificant(value: number, digits: number): string {
  return Number.isFinite(value) ? String(Number(value.toPrecision(digits))) : '';
}

/** A model limit as text: '−∞' / '+∞' for an absent bound, '—' for NaN, otherwise the shortest exact text. */
export function formatLimit(value: number): string {
  if (Number.isNaN(value)) {
    return '—';
  }
  if (value === Number.POSITIVE_INFINITY) {
    return '+∞';
  }
  return value === Number.NEGATIVE_INFINITY ? '−∞' : formatNumber(value);
}
