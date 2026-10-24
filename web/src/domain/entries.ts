import type { TirEntry } from './model';

/** Groups entries by section, in order of first appearance. */
export function groupBySection(entries: readonly TirEntry[]): Array<{ section: string; entries: TirEntry[] }> {
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
