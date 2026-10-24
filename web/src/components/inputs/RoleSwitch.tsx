import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group';
import { Tooltip, TooltipContent, TooltipTrigger } from '@/components/ui/tooltip';
import { INPUT_ROLES, type InputRole } from '@/domain/roles';

const LABELS: Readonly<Record<InputRole, string>> = {
  sweep: 'Sweep',
  step: 'Step',
  value: 'Value',
};

const HINTS: Readonly<Record<InputRole, string>> = {
  sweep: 'Plot against this input',
  step: 'One line per value',
  value: 'Hold at one value',
};

interface RoleSwitchProps {
  name: string;
  role: InputRole;
  blockers: Readonly<Record<InputRole, string | null>>;
  onRole: (role: InputRole) => void;
}

export function RoleSwitch({ name, role, blockers, onRole }: RoleSwitchProps) {
  return (
    <ToggleGroup
      type="single"
      variant="segmented"
      value={role}
      aria-label={`${name} role`}
      data-testid={`role-switch-${name}`}
      onValueChange={(next) => {
        if (next === 'sweep' || next === 'step' || next === 'value') {
          onRole(next);
        }
      }}
    >
      {INPUT_ROLES.map((r) => (
        <Tooltip key={r} delayDuration={300}>
          <TooltipTrigger asChild>
            <span className="inline-flex" data-testid={`role-option-${name}-${r}`}>
              <ToggleGroupItem
                value={r}
                disabled={blockers[r] !== null}
                data-testid={`role-${name}-${r}`}
              >
                {LABELS[r]}
              </ToggleGroupItem>
            </span>
          </TooltipTrigger>
          <TooltipContent>{blockers[r] ?? HINTS[r]}</TooltipContent>
        </Tooltip>
      ))}
    </ToggleGroup>
  );
}
