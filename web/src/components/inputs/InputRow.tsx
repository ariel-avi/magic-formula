import { useId } from 'react';
import { NumberField } from '@/components/NumberField';
import { QuantitySymbol } from '@/components/QuantitySymbol';
import { Label } from '@/components/ui/label';
import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group';
import { channelLabel } from '@/domain/channels';
import type { ChartSpec } from '@/domain/chart-spec';
import type { InputChannel, TirEntry } from '@/domain/model';
import { assignRole, roleBlocker, roleOf } from '@/domain/roles';
import { setColourMode } from '@/domain/steps';
import { displayUnit, hasUnit } from '@/domain/units';
import { cn } from '@/lib/utils';
import { RoleSwitch } from './RoleSwitch';
import { StepValuesInput } from './StepValuesInput';

interface InputRowProps {
  channel: InputChannel;
  entries: readonly TirEntry[];
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
}

export function InputRow({ channel, entries, spec, onChange }: InputRowProps) {
  const name = channel.name;
  const unit = displayUnit(channel.unit).label;
  const role = roleOf(spec, name);
  const blockers = {
    sweep: roleBlocker(spec, name, 'sweep'),
    step: roleBlocker(spec, name, 'step'),
    value: roleBlocker(spec, name, 'value'),
  };
  const label = channelLabel(name);
  const startId = useId();
  const endId = useId();
  const pointsId = useId();

  const setSweep = (field: 'start' | 'end' | 'points', v: number) =>
    onChange({ ...spec, sweep: { ...spec.sweep, [field]: v } });

  const k = spec.steps.findIndex((s) => s.variable === name);
  const step = k >= 0 ? spec.steps[k] : undefined;

  return (
    <div
      data-testid={`input-row-${name}`}
      data-role={role}
      className={cn(
        'border-l-2 px-3 py-2',
        role === 'sweep'
          ? 'border-l-primary'
          : role === 'step'
            ? 'border-l-foreground/40'
            : 'border-l-transparent',
      )}
    >
      <div className="grid h-9 grid-cols-[2rem_1fr_auto] items-center gap-2">
        <QuantitySymbol name={name} className="text-lg" />
        <div className="min-w-0 truncate">
          <span className="font-mono text-xs font-medium">{name}</span>{' '}
          <span className="text-xs text-muted-foreground">{label.description}</span>
          {hasUnit(unit) && <span className="ml-1 text-xs text-muted-foreground">[{unit}]</span>}
        </div>
        <RoleSwitch
          name={name}
          role={role}
          blockers={blockers}
          onRole={(next) => onChange(assignRole(spec, channel, next, entries))}
        />
      </div>
      <div className="pt-1 pl-10">
        {role === 'sweep' && (
          <div className="grid grid-cols-3 gap-2">
            <div className="space-y-1">
              <Label htmlFor={startId} className="text-[11px] text-muted-foreground">
                Start
              </Label>
              <NumberField
                id={startId}
                label={`${name} sweep start`}
                data-testid="sweep-start"
                unit={unit}
                value={spec.sweep.start}
                onValue={(v) => setSweep('start', v)}
              />
            </div>
            <div className="space-y-1">
              <Label htmlFor={endId} className="text-[11px] text-muted-foreground">
                End
              </Label>
              <NumberField
                id={endId}
                label={`${name} sweep end`}
                data-testid="sweep-end"
                unit={unit}
                value={spec.sweep.end}
                onValue={(v) => setSweep('end', v)}
              />
            </div>
            <div className="space-y-1">
              <Label htmlFor={pointsId} className="text-[11px] text-muted-foreground">
                Points
              </Label>
              <NumberField
                id={pointsId}
                label={`${name} sweep points`}
                data-testid="sweep-points"
                value={spec.sweep.points}
                onValue={(v) => setSweep('points', v)}
              />
            </div>
          </div>
        )}
        {role === 'step' && step !== undefined && (
          <div className="space-y-2">
            <StepValuesInput
              name={name}
              unit={unit}
              values={step.values}
              onValues={(values) =>
                onChange({
                  ...spec,
                  steps: spec.steps.map((s, i) => (i === k ? { ...s, values } : s)),
                })
              }
            />
            <div className="flex items-center gap-2">
              <span className="text-[11px] text-muted-foreground">Colour</span>
              <ToggleGroup
                type="single"
                variant="segmented"
                aria-label={`${name} colour mode`}
                data-testid={`colour-mode-${name}`}
                value={step.colourMode}
                onValueChange={(next) => {
                  if (next === 'hue' || next === 'saturation') {
                    onChange({ ...spec, steps: setColourMode(spec.steps, k, next) });
                  }
                }}
              >
                <ToggleGroupItem value="hue">Hue</ToggleGroupItem>
                <ToggleGroupItem value="saturation">Saturation</ToggleGroupItem>
              </ToggleGroup>
            </div>
          </div>
        )}
        {role === 'value' && (
          <NumberField
            label={`${name} value`}
            data-testid={`value-${name}`}
            unit={unit}
            value={spec.fixed[name] ?? Number.NaN}
            onValue={(v) => onChange({ ...spec, fixed: { ...spec.fixed, [name]: v } })}
            className="w-40"
          />
        )}
      </div>
    </div>
  );
}
