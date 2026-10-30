import { useId } from 'react';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { MAX_SWEEP_NAME_LENGTH, SWEEP_NAME_PREFIX, type SweepSpec } from '@/domain/chart-spec';

interface SweepSettingsProps {
  sweep: SweepSpec;
  index: number;
  onRename: (name: string) => void;
  onNameBlur: () => void;
  onUncombined: (uncombined: boolean) => void;
  supportsTurnSlip: boolean;
  onTurnSlip: (enabled: boolean) => void;
}

export function SweepSettings({ sweep, index, onRename, onNameBlur, onUncombined, supportsTurnSlip, onTurnSlip }: SweepSettingsProps) {
  const nameId = useId();
  const switchId = useId();
  const turnSlipId = useId();
  return (
    <div className="flex flex-wrap items-center gap-x-3 gap-y-2 border-b px-3 py-2">
      <div className="flex min-w-0 grow basis-72 items-center gap-2">
        <Label htmlFor={nameId} className="text-xs">
          Name
        </Label>
        <Input
          id={nameId}
          data-testid="sweep-name"
          aria-label="Sweep name"
          maxLength={MAX_SWEEP_NAME_LENGTH}
          placeholder={`${SWEEP_NAME_PREFIX}${index + 1}`}
          value={sweep.name}
          onChange={(e) => onRename(e.target.value)}
          onBlur={onNameBlur}
          className="h-7 min-w-0 flex-1 text-xs md:text-xs"
        />
      </div>
      {supportsTurnSlip && (
        <div className="flex items-center gap-2">
          <Switch id={turnSlipId} data-testid="turn-slip-switch" checked={sweep.turnSlip ?? false} onCheckedChange={onTurnSlip} />
          <Label htmlFor={turnSlipId} className="text-xs">Turn slip</Label>
        </div>
      )}
      <div className="flex items-center gap-2">
        <Switch
          id={switchId}
          data-testid="uncombined-switch"
          checked={sweep.uncombined}
          onCheckedChange={onUncombined}
        />
        <Label htmlFor={switchId} className="text-xs">
          Pure slip (uncombined)
        </Label>
      </div>
    </div>
  );
}
