import { useId, useState } from 'react';
import { Input } from '@/components/ui/input';
import { hasUnit } from '@/domain/units';
import { formatNumber, parseNumber } from '@/domain/numbers';
import { cn } from '@/lib/utils';

interface NumberFieldProps {
  value: number;
  onValue: (value: number) => void;
  /** Called with false while the text is not a valid number, true otherwise. */
  onValidity?: (valid: boolean) => void;
  label: string;
  unit?: string;
  id?: string;
  className?: string;
  'data-testid'?: string;
}

/** A numeric text field: valid numbers are applied on every keystroke, anything else shows an error and is not applied. */
export function NumberField({
  value,
  onValue,
  onValidity,
  label,
  unit,
  id,
  className,
  'data-testid': testId,
}: NumberFieldProps) {
  const errorId = useId();
  const [draft, setDraft] = useState(formatNumber(value));
  const [error, setError] = useState<string | null>(null);
  const [seen, setSeen] = useState(value);

  if (!Object.is(value, seen)) {
    setSeen(value);
    setDraft(formatNumber(value));
    setError(null);
  }

  const showUnit = unit !== undefined && hasUnit(unit);

  return (
    <div className={cn('relative', className)}>
      <Input
        id={id}
        type="text"
        inputMode="decimal"
        aria-label={label}
        data-testid={testId}
        aria-invalid={error !== null}
        aria-describedby={error !== null ? errorId : undefined}
        className={cn(
          'h-7 text-right font-mono text-xs md:text-xs tabular-nums slashed-zero',
          showUnit ? 'pr-9' : '',
        )}
        value={draft}
        onChange={(event) => {
          const text = event.target.value;
          setDraft(text);
          const parsed = parseNumber(text);
          if (parsed.ok) {
            setError(null);
            setSeen(parsed.value);
            onValidity?.(true);
            onValue(parsed.value);
          } else {
            setError(parsed.error);
            onValidity?.(false);
          }
        }}
        onKeyDown={(event) => {
          if (event.key === 'Escape') {
            setDraft(formatNumber(value));
            setError(null);
            onValidity?.(true);
          }
        }}
      />
      {showUnit && (
        <span
          aria-hidden="true"
          className="pointer-events-none absolute inset-y-0 right-2 flex h-7 items-center text-[11px] text-muted-foreground"
        >
          {unit}
        </span>
      )}
      {error !== null && (
        <p id={errorId} className="mt-0.5 text-xs text-destructive">
          {error}
        </p>
      )}
    </div>
  );
}
