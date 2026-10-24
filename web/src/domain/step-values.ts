import { MAX_STEP_VALUES } from './chart-spec';
import { formatNumber } from './numbers';
import { err, ok, type Result } from './result';
import { linspace, parseValueList } from './sweep';

const ascending = (a: number, b: number) => a - b;

/** Adds every number in `text` to `values`; the result is sorted ascending. Duplicates are rejected. */
export function addStepValues(values: readonly number[], text: string): Result<number[], string> {
  const parsed = parseValueList(text);
  if (!parsed.ok) {
    return err(parsed.error);
  }
  const merged = [...values];
  for (const value of parsed.value) {
    if (merged.includes(value)) {
      return err(`${formatNumber(value)} is already in the list.`);
    }
    merged.push(value);
  }
  if (merged.length > MAX_STEP_VALUES) {
    return err(`A step takes at most ${MAX_STEP_VALUES} values.`);
  }
  return ok(merged.sort(ascending));
}

/** `values` without index `index`; the last remaining value is kept. */
export function removeStepValue(values: readonly number[], index: number): number[] {
  return values.length <= 1 ? [...values] : values.filter((_, i) => i !== index);
}

/** `count` evenly spaced values from `start` to `end`, rounded for clean display and sorted ascending. */
export function stepRange(start: number, end: number, count: number): Result<number[], string> {
  if (!Number.isInteger(count) || count < 2 || count > MAX_STEP_VALUES) {
    return err(`Count must be an integer from 2 to ${MAX_STEP_VALUES}.`);
  }
  if (!Number.isFinite(start) || !Number.isFinite(end) || start === end) {
    return err('From and to must be finite and different.');
  }
  return ok(
    linspace(start, end, count)
      .map((value) => Number(value.toPrecision(12)))
      .sort(ascending),
  );
}
