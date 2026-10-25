import { CopyIcon, PlusIcon, Trash2Icon } from 'lucide-react';
import { DashSwatch } from '@/components/DashSwatch';
import { Button } from '@/components/ui/button';
import { TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Tooltip, TooltipContent, TooltipTrigger } from '@/components/ui/tooltip';
import { MAX_SWEEPS, type SweepSpec } from '@/domain/chart-spec';
import { sweepDisplayName } from '@/domain/chart';
import { SWEEP_DASHES } from '@/domain/colours';

interface SweepToolbarProps {
  sweeps: readonly SweepSpec[];
  onAdd: () => void;
  onDuplicate: () => void;
  onRemove: () => void;
}

interface ToolButtonProps {
  testId: string;
  label: string;
  disabled: boolean;
  tooltip: string;
  onClick: () => void;
  children: React.ReactNode;
}

function ToolButton({ testId, label, disabled, tooltip, onClick, children }: ToolButtonProps) {
  return (
    <Tooltip delayDuration={300}>
      <TooltipTrigger asChild>
        <span className="inline-flex">
          <Button
            variant="ghost"
            size="icon"
            className="size-7"
            data-testid={testId}
            aria-label={label}
            disabled={disabled}
            onClick={onClick}
          >
            {children}
          </Button>
        </span>
      </TooltipTrigger>
      <TooltipContent>{tooltip}</TooltipContent>
    </Tooltip>
  );
}

export function SweepToolbar({ sweeps, onAdd, onDuplicate, onRemove }: SweepToolbarProps) {
  const full = sweeps.length >= MAX_SWEEPS;
  const limit = `At most ${MAX_SWEEPS} sweeps`;
  return (
    <div className="flex items-start gap-1 border-b px-3 py-2">
      <TabsList data-testid="sweep-tabs" className="h-auto min-w-0 flex-1 flex-wrap justify-start">
        {sweeps.map((s, i) => (
          <TabsTrigger
            key={s.id}
            data-testid="sweep-tab"
            data-sweep={s.id}
            value={s.id}
            className="h-auto max-w-full flex-none text-left text-xs whitespace-normal [overflow-wrap:anywhere]"
          >
            <DashSwatch colour="currentColor" dash={SWEEP_DASHES[i]} />
            {sweepDisplayName(s, i)}
          </TabsTrigger>
        ))}
      </TabsList>
      <div className="flex shrink-0 items-center gap-0.5">
        <ToolButton
          testId="sweep-add"
          label="New sweep"
          disabled={full}
          tooltip={full ? limit : 'New sweep'}
          onClick={onAdd}
        >
          <PlusIcon />
        </ToolButton>
        <ToolButton
          testId="sweep-duplicate"
          label="Duplicate sweep"
          disabled={full}
          tooltip={full ? limit : 'Duplicate sweep'}
          onClick={onDuplicate}
        >
          <CopyIcon />
        </ToolButton>
        <ToolButton
          testId="sweep-remove"
          label="Remove sweep"
          disabled={sweeps.length <= 1}
          tooltip={sweeps.length <= 1 ? 'A chart needs at least one sweep' : 'Remove sweep'}
          onClick={onRemove}
        >
          <Trash2Icon />
        </ToolButton>
      </div>
    </div>
  );
}
