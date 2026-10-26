import { useId, useState } from 'react';
import { Input } from '@/components/ui/input';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { entryKind } from '@/domain/entries';
import type { TirEntry } from '@/domain/model';
import { parseNumber } from '@/domain/numbers';
import { unitOptions } from '@/domain/tir-units';

interface EntryFieldProps {
  entry: TirEntry;
  /** Returns an error message, or null when the edit was applied. */
  onEdit: (entry: TirEntry, value: string) => string | null;
}

function FieldError({ id, entry, error }: { id: string; entry: TirEntry; error: string | null }) {
  if (error === null) {
    return null;
  }
  return (
    <p id={id} data-testid={`param-error-${entry.section}-${entry.key}`} className="mt-0.5 text-xs text-destructive">
      {error}
    </p>
  );
}

function NumberEntry({ entry, onEdit }: EntryFieldProps) {
  const [draft, setDraft] = useState(entry.value);
  const [error, setError] = useState<string | null>(null);
  const errorId = useId();

  const commit = (source: 'enter' | 'blur') => {
    const next = draft.trim();
    if (next === entry.value) {
      return;
    }
    const parsed = parseNumber(draft);
    if (!parsed.ok) {
      setError(parsed.error);
      if (source === 'blur') {
        setDraft(entry.value);
      }
      return;
    }
    const message = onEdit(entry, next);
    if (message !== null) {
      setError(message);
      setDraft(entry.value);
    } else {
      setError(null);
    }
  };

  return (
    <div>
      <Input
        inputMode="decimal"
        aria-label={`${entry.section} ${entry.key}`}
        aria-invalid={error !== null}
        aria-describedby={error !== null ? errorId : undefined}
        className="h-7 text-right font-mono text-xs md:text-xs tabular-nums slashed-zero"
        value={draft}
        onChange={(event) => {
          setDraft(event.target.value);
          setError(null);
        }}
        onBlur={() => commit('blur')}
        onKeyDown={(event) => {
          if (event.key === 'Enter') {
            commit('enter');
          } else if (event.key === 'Escape') {
            setDraft(entry.value);
            setError(null);
          }
        }}
      />
      <FieldError id={errorId} entry={entry} error={error} />
    </div>
  );
}

function UnitEntry({ entry, onEdit }: EntryFieldProps) {
  const [error, setError] = useState<string | null>(null);
  const errorId = useId();
  const options = unitOptions(entry.key, entry.value);

  return (
    <div>
      <Select
        value={entry.value}
        onValueChange={(value) => {
          if (value !== entry.value) {
            setError(onEdit(entry, value));
          }
        }}
      >
        <SelectTrigger
          size="sm"
          aria-label={`${entry.section} ${entry.key}`}
          aria-invalid={error !== null}
          aria-describedby={error !== null ? errorId : undefined}
          data-testid={`unit-select-${entry.key}`}
          className="data-[size=sm]:h-7 w-full font-mono text-xs"
        >
          <SelectValue placeholder="Not set" />
        </SelectTrigger>
        <SelectContent>
          {options.map((option) => (
            <SelectItem
              key={option.value}
              value={option.value}
              disabled={!option.supported}
              description={option.supported ? undefined : 'not supported yet'}
              className="font-mono text-xs"
            >
              {option.value}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
      <FieldError id={errorId} entry={entry} error={error} />
    </div>
  );
}

function TextEntry({ entry, onEdit }: EntryFieldProps) {
  const [draft, setDraft] = useState(entry.value);
  const [error, setError] = useState<string | null>(null);
  const errorId = useId();

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
    <div>
      <Input
        aria-label={`${entry.section} ${entry.key}`}
        aria-invalid={error !== null}
        aria-describedby={error !== null ? errorId : undefined}
        className="h-7 font-mono text-xs md:text-xs"
        value={draft}
        onChange={(event) => setDraft(event.target.value)}
        onBlur={commit}
        onKeyDown={(event) => {
          if (event.key === 'Enter') {
            commit();
          } else if (event.key === 'Escape') {
            setDraft(entry.value);
            setError(null);
          }
        }}
      />
      <FieldError id={errorId} entry={entry} error={error} />
    </div>
  );
}

export function EntryField({ entry, onEdit }: EntryFieldProps): React.JSX.Element {
  switch (entryKind(entry)) {
    case 'number':
      return <NumberEntry entry={entry} onEdit={onEdit} />;
    case 'unit':
      return <UnitEntry entry={entry} onEdit={onEdit} />;
    case 'text':
      return <TextEntry entry={entry} onEdit={onEdit} />;
  }
}
