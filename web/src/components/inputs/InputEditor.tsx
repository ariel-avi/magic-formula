import { ScrollArea } from '@/components/ui/scroll-area';
import { MAX_STEPS, type ChartSpec } from '@/domain/chart-spec';
import type { ModelInfo, TirEntry } from '@/domain/model';
import { InputRow } from './InputRow';

interface InputEditorProps {
  model: ModelInfo;
  entries: readonly TirEntry[];
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
}

export function InputEditor({ model, entries, spec, onChange }: InputEditorProps) {
  return (
    <div data-testid="input-editor" className="flex min-h-0 flex-col xl:flex-1">
      <div className="border-b px-3 py-2">
        <h2 className="text-[15px] font-semibold">Inputs</h2>
        <p className="text-xs text-muted-foreground">{`One input is swept; up to ${MAX_STEPS} are stepped.`}</p>
      </div>
      <ScrollArea className="xl:min-h-0 xl:flex-1">
        <div className="divide-y">
          {model.inputs.map((channel) => (
            <InputRow
              key={channel.name}
              channel={channel}
              entries={entries}
              spec={spec}
              onChange={onChange}
            />
          ))}
        </div>
      </ScrollArea>
    </div>
  );
}
