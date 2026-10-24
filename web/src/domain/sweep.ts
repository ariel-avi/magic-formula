import { err, ok, type Result } from './result';

/** `points` values from `start` to `end`, both included. The range may be descending. */
export function linspace(start: number, end: number, points: number): number[] {
  if (!Number.isInteger(points) || points < 2) {
    throw new RangeError(`linspace needs an integer point count of at least 2, got ${points}`);
  }
  const last = points - 1;
  return Array.from({ length: points }, (_, i) => (i === last ? end : start + ((end - start) * i) / last));
}

const NUMBER_TOKEN = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;

/** Parses a list of numbers separated by commas, blanks or tabs. */
export function parseValueList(text: string): Result<number[], string> {
  const trimmed = text.trim();
  if (trimmed === '') {
    return err('Enter at least one value.');
  }
  const values: number[] = [];
  for (const token of trimmed.split(/\s*,\s*|\s+/)) {
    if (token === '') {
      return err('Empty value in the list.');
    }
    if (!NUMBER_TOKEN.test(token) || !Number.isFinite(Number(token))) {
      return err(`'${token}' is not a finite number.`);
    }
    values.push(Number(token));
  }
  return ok(values);
}
