import type { TirEntry } from './model';
import { parseNumber } from './numbers';

export type EntryKind = 'number' | 'unit' | 'text';

/** 'unit' for entries of the UNITS section, 'number' for unquoted entries that parse as a number, 'text' otherwise. */
export function entryKind(entry: TirEntry): EntryKind {
  if (entry.section.toUpperCase() === 'UNITS') {
    return 'unit';
  }
  return !entry.quoted && parseNumber(entry.value).ok ? 'number' : 'text';
}

export interface EntryGroup {
  section: string;
  entries: TirEntry[];
}

/** Groups with only the entries whose key contains `filter` (case-insensitive); empty groups are dropped. */
export function filterGroups(groups: readonly EntryGroup[], filter: string): EntryGroup[] {
  const needle = filter.trim().toUpperCase();
  if (needle === '') {
    return groups.slice();
  }
  return groups
    .map((group) => ({
      section: group.section,
      entries: group.entries.filter((entry) => entry.key.toUpperCase().includes(needle)),
    }))
    .filter((group) => group.entries.length > 0);
}

/** Groups entries by section, in order of first appearance. */
export function groupBySection(entries: readonly TirEntry[]): EntryGroup[] {
  const groups = new Map<string, TirEntry[]>();
  for (const entry of entries) {
    const group = groups.get(entry.section);
    if (group === undefined) {
      groups.set(entry.section, [entry]);
    } else {
      group.push(entry);
    }
  }
  return [...groups].map(([section, grouped]) => ({ section, entries: grouped }));
}

const entryId = (entry: TirEntry) => `${entry.section}/${entry.key}`;

/** Ids ('SECTION/KEY') of the current entries that differ from the original or are new. */
export function changedEntryIds(original: readonly TirEntry[], current: readonly TirEntry[]): Set<string> {
  const before = new Map(original.map((entry) => [entryId(entry), entry]));
  const changed = new Set<string>();
  for (const entry of current) {
    const previous = before.get(entryId(entry));
    if (previous === undefined || previous.value !== entry.value || previous.quoted !== entry.quoted) {
      changed.add(entryId(entry));
    }
  }
  return changed;
}

/** Numeric value of the first entry named `key`; null when absent, empty or not a finite number. */
export function findNumber(entries: readonly TirEntry[], key: string): number | null {
  const entry = entries.find((candidate) => candidate.key === key);
  if (entry === undefined) {
    return null;
  }
  const text = entry.value.trim();
  if (text === '') {
    return null;
  }
  const value = Number(text);
  return Number.isFinite(value) ? value : null;
}
