import { useId, useState } from 'react';
import { PlusIcon, XIcon } from 'lucide-react';
import { NumberField } from '@/components/NumberField';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover';
import { formatNumber } from '@/domain/numbers';
import { addStepValues, removeStepValue, stepRange } from '@/domain/step-values';
import { hasUnit } from '@/domain/units';

interface StepValuesInputProps {
  name: string;
  unit: string;
  values: readonly number[];
  onValues: (values: number[]) => void;
}

export function StepValuesInput({ name, unit, values, onValues }: StepValuesInputProps) {
  const errorId = useId();
  const fromId = useId();
  const toId = useId();
  const countId = useId();
  const [draft, setDraft] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [open, setOpen] = useState(false);
  const [from, setFrom] = useState(0);
  const [to, setTo] = useState(0);
  const [count, setCount] = useState(2);
  const [invalid, setInvalid] = useState({ from: false, to: false, count: false });

  const add = () => {
    const result = addStepValues(values, draft);
    if (result.ok) {
      onValues(result.value);
      setDraft('');
      setError(null);
    } else {
      setError(result.error);
    }
  };

  const range = stepRange(from, to, count);

  return (
    <div data-testid={`step-values-${name}`} className="space-y-1.5">
      <div className="flex flex-wrap gap-1">
        {values.map((v, i) => (
          <Badge
            key={formatNumber(v)}
            variant="outline"
            data-testid="step-chip"
            data-value={formatNumber(v)}
            className="h-6 gap-1 rounded-sm pr-0.5 pl-2 font-mono tabular-nums slashed-zero"
          >
            {formatNumber(v)}
            {hasUnit(unit) && <span className="text-muted-foreground">{unit}</span>}
            <Button
              variant="ghost"
              size="icon"
              className="size-5"
              aria-label={`Remove ${formatNumber(v)}`}
              disabled={values.length === 1}
              onClick={() => onValues(removeStepValue(values, i))}
            >
              <XIcon className="size-3" />
            </Button>
          </Badge>
        ))}
      </div>
      <div className="flex items-center gap-1">
        <Input
          aria-label={`${name} step value`}
          data-testid={`step-add-${name}`}
          placeholder="Add value"
          inputMode="decimal"
          className="h-7 min-w-0 flex-1 font-mono text-xs md:text-xs"
          aria-invalid={error !== null}
          aria-describedby={error !== null ? errorId : undefined}
          value={draft}
          onChange={(event) => {
            setDraft(event.target.value);
            setError(null);
          }}
          onKeyDown={(event) => {
            if (event.key === 'Enter') {
              event.preventDefault();
              add();
            } else if (event.key === 'Escape') {
              setDraft('');
              setError(null);
            }
          }}
        />
        <Button variant="ghost" size="icon" className="size-7" aria-label="Add value" onClick={add}>
          <PlusIcon />
        </Button>
        <Popover
          open={open}
          onOpenChange={(next) => {
            if (next) {
              setFrom(values[0]);
              setTo(values[values.length - 1]);
              setCount(Math.max(2, values.length));
              setInvalid({ from: false, to: false, count: false });
            }
            setOpen(next);
          }}
        >
          <PopoverTrigger asChild>
            <Button variant="ghost" size="sm" className="h-7 text-xs">
              Range…
            </Button>
          </PopoverTrigger>
          <PopoverContent align="start" className="w-72 space-y-3">
            <div className="grid grid-cols-[auto_minmax(0,1fr)] items-center gap-x-3 gap-y-2">
              <Label htmlFor={fromId} className="text-[11px] text-muted-foreground">
                From
              </Label>
              <NumberField
                id={fromId}
                label={`${name} range from`}
                unit={unit}
                value={from}
                onValue={setFrom}
                onValidity={(valid) => setInvalid((s) => ({ ...s, from: !valid }))}
              />
              <Label htmlFor={toId} className="text-[11px] text-muted-foreground">
                To
              </Label>
              <NumberField
                id={toId}
                label={`${name} range to`}
                unit={unit}
                value={to}
                onValue={setTo}
                onValidity={(valid) => setInvalid((s) => ({ ...s, to: !valid }))}
              />
              <Label htmlFor={countId} className="text-[11px] text-muted-foreground">
                Count
              </Label>
              <NumberField
                id={countId}
                label={`${name} range count`}
                value={count}
                onValue={setCount}
                onValidity={(valid) => setInvalid((s) => ({ ...s, count: !valid }))}
              />
            </div>
            {!range.ok && (
              <p data-testid={`step-range-error-${name}`} className="text-xs text-destructive">
                {range.error}
              </p>
            )}
            <Button
              size="sm"
              disabled={!range.ok || invalid.from || invalid.to || invalid.count}
              onClick={() => {
                if (range.ok && !invalid.from && !invalid.to && !invalid.count) {
                  onValues(range.value);
                  setOpen(false);
                }
              }}
            >
              Fill
            </Button>
          </PopoverContent>
        </Popover>
      </div>
      {error !== null && (
        <p id={errorId} data-testid={`step-error-${name}`} className="text-xs text-destructive">
          {error}
        </p>
      )}
    </div>
  );
}
