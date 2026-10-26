import { useId } from 'react';
import { Label } from '@/components/ui/label';
import {
  Select,
  SelectContent,
  SelectGroup,
  SelectItem,
  SelectLabel,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select';
import { axisKey, type AxisOption } from '@/domain/axes';
import type { AxisRef } from '@/domain/chart-spec';

interface AxisSelectProps {
  axis: 'x' | 'y';
  options: readonly AxisOption[];
  value: AxisRef;
  onChange: (ref: AxisRef) => void;
}

export function AxisSelect({ axis, options, value, onChange }: AxisSelectProps) {
  const id = useId();
  const groups = [
    { title: 'Inputs', kind: 'input' },
    { title: 'Outputs', kind: 'output' },
  ] as const;
  return (
    <div className="flex items-center gap-1.5">
      <Label htmlFor={id} className="text-xs text-muted-foreground">
        {axis === 'x' ? 'X' : 'Y'}
      </Label>
      <Select
        value={axisKey(value)}
        onValueChange={(key) => {
          const o = options.find((option) => option.key === key);
          if (o !== undefined) onChange(o.ref);
        }}
      >
        <SelectTrigger
          id={id}
          size="sm"
          aria-label={axis === 'x' ? 'X axis' : 'Y axis'}
          data-testid={`${axis}-axis-select`}
          className="min-w-36 font-mono text-xs"
        >
          <SelectValue />
        </SelectTrigger>
        <SelectContent>
          {groups.map((group) => (
            <SelectGroup key={group.kind}>
              <SelectLabel>{group.title}</SelectLabel>
              {options
                .filter((o) => o.ref.kind === group.kind)
                .map((o) => (
                  <SelectItem key={o.key} value={o.key} description={o.description} className="font-mono text-xs">
                    {o.label}
                  </SelectItem>
                ))}
            </SelectGroup>
          ))}
        </SelectContent>
      </Select>
    </div>
  );
}
