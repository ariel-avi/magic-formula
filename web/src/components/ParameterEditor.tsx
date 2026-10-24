import { useMemo, useState } from 'react';
import { Badge } from '@/components/ui/badge';
import { Input } from '@/components/ui/input';
import { changedEntryIds, groupBySection } from '@/domain/entries';
import type { TirEntry } from '@/domain/model';

interface Props {
  original: readonly TirEntry[];
  entries: readonly TirEntry[];
  /** Returns an error message, or null when the edit was applied. */
  onEdit: (entry: TirEntry, value: string) => string | null;
}

function EntryRow({ entry, changed, onEdit }: { entry: TirEntry; changed: boolean; onEdit: Props['onEdit'] }) {
  const [draft, setDraft] = useState(entry.value);
  const [error, setError] = useState<string | null>(null);

  const commit = () => {
    if (draft === entry.value) {
      return;
    }
    const message = onEdit(entry, draft);
    setError(message);
    if (message !== null) {
      setDraft(entry.value);
    }
  };

  return (
    <div className="grid grid-cols-[14rem_1fr_auto] items-center gap-2 py-0.5">
      <span className={entry.used ? 'font-mono text-sm' : 'font-mono text-sm text-muted-foreground'}>
        {entry.key}
        {!entry.used && ' (unused)'}
      </span>
      <div>
        <Input
          className={changed ? 'h-8 border-amber-500 font-mono' : 'h-8 font-mono'}
          value={draft}
          aria-label={`${entry.section} ${entry.key}`}
          onChange={(event) => setDraft(event.target.value)}
          onBlur={commit}
          onKeyDown={(event) => {
            if (event.key === 'Enter') {
              commit();
            }
          }}
        />
        {error !== null && <p className="text-xs text-destructive">{error}</p>}
      </div>
      {changed ? <Badge variant="secondary">edited</Badge> : <span />}
    </div>
  );
}

export function ParameterEditor({ original, entries, onEdit }: Props) {
  const [filter, setFilter] = useState('');
  const changed = useMemo(() => changedEntryIds(original, entries), [original, entries]);
  const groups = useMemo(() => groupBySection(entries), [entries]);
  const needle = filter.trim().toUpperCase();

  return (
    <div className="space-y-4">
      <Input placeholder="Filter keys (e.g. PCY1)" value={filter} onChange={(event) => setFilter(event.target.value)} />
      {groups.map((group) => {
        const rows = group.entries.filter((entry) => needle === '' || entry.key.includes(needle));
        if (rows.length === 0) {
          return null;
        }
        return (
          <section key={group.section}>
            <h3 className="mb-1 border-b pb-1 text-sm font-semibold">[{group.section}]</h3>
            {rows.map((entry) => (
              <EntryRow
                key={`${entry.section}/${entry.key}/${entry.value}`}
                entry={entry}
                changed={changed.has(`${entry.section}/${entry.key}`)}
                onEdit={onEdit}
              />
            ))}
          </section>
        );
      })}
    </div>
  );
}
