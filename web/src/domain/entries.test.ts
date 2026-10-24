import { describe, expect, it } from 'vitest';
import { changedEntryIds, findNumber, groupBySection } from './entries';
import type { TirEntry } from './model';

const entry = (section: string, key: string, value: string, quoted = false): TirEntry => ({
  section,
  key,
  value,
  quoted,
  line: 1,
  used: true,
});

describe('groupBySection', () => {
  it('keeps file order and merges non-adjacent entries of a section', () => {
    const groups = groupBySection([entry('A', 'K1', '1'), entry('B', 'K2', '2'), entry('A', 'K3', '3')]);
    expect(groups.map((g) => g.section)).toEqual(['A', 'B']);
    expect(groups[0].entries.map((e) => e.key)).toEqual(['K1', 'K3']);
  });

  it('returns nothing for no entries', () => {
    expect(groupBySection([])).toEqual([]);
  });
});

describe('changedEntryIds', () => {
  const original = [entry('S', 'A', '1'), entry('S', 'B', 'x', true), entry('S', 'C', '3')];

  it('reports changed values, changed quoting and new entries only', () => {
    const current = [entry('S', 'A', '2'), entry('S', 'B', 'x', false), entry('S', 'C', '3'), entry('S', 'D', '4')];
    expect([...changedEntryIds(original, current)].sort()).toEqual(['S/A', 'S/B', 'S/D']);
  });

  it('reports nothing when unchanged', () => {
    expect(changedEntryIds(original, original).size).toBe(0);
  });
});

describe('findNumber', () => {
  const entries = [entry('S', 'A', ' 4000 '), entry('S', 'B', 'abc'), entry('S', 'C', '  ')];

  it('parses a numeric value', () => {
    expect(findNumber(entries, 'A')).toBe(4000);
  });

  it('returns null for absent, non-numeric and empty values', () => {
    expect(findNumber(entries, 'Z')).toBeNull();
    expect(findNumber(entries, 'B')).toBeNull();
    expect(findNumber(entries, 'C')).toBeNull();
  });
});
