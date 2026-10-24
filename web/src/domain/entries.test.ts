import { describe, expect, it } from 'vitest';
import { changedEntryIds, entryKind, filterGroups, findNumber, groupBySection } from './entries';
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

describe('entryKind', () => {
  it('marks entries of the UNITS section as units, quoted or not', () => {
    expect(entryKind(entry('UNITS', 'LENGTH', 'meter', true))).toBe('unit');
    expect(entryKind(entry('UNITS', 'LENGTH', 'meter'))).toBe('unit');
    expect(entryKind(entry('units', 'LENGTH', 'meter'))).toBe('unit');
  });

  it('marks quoted entries as text and the rest as numbers', () => {
    expect(entryKind(entry('MODEL', 'TYRESIDE', 'LEFT', true))).toBe('text');
    expect(entryKind(entry('MODEL', 'FITTYP', '6'))).toBe('number');
  });

  it('marks an unquoted value that is not a number as text', () => {
    expect(entryKind(entry('MODEL', 'TYRESIDE', 'LEFT'))).toBe('text');
  });
});

describe('filterGroups', () => {
  const groups = groupBySection([entry('A', 'PDY1', '1'), entry('A', 'PCY1', '2'), entry('B', 'QSY1', '3')]);

  it('returns all groups for a blank filter', () => {
    expect(filterGroups(groups, '')).toEqual(groups);
    expect(filterGroups(groups, '   ')).toEqual(groups);
  });

  it('matches keys case-insensitively and drops empty groups', () => {
    const result = filterGroups(groups, 'pdy');
    expect(result).toHaveLength(1);
    expect(result[0].section).toBe('A');
    expect(result[0].entries.map((e) => e.key)).toEqual(['PDY1']);
  });

  it('returns nothing when no key matches', () => {
    expect(filterGroups(groups, 'zzz')).toEqual([]);
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
