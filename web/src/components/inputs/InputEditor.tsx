import { useState } from 'react';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Tabs, TabsContent } from '@/components/ui/tabs';
import {
  addSweep,
  duplicateSweep,
  freezeBlankName,
  removeSweep,
  renameSweep,
  replaceSweep,
  sweepIndexOf,
} from '@/domain/chart';
import { MAX_STEPS, type ChartSpec } from '@/domain/chart-spec';
import type { ModelInfo, TirEntry } from '@/domain/model';
import { InputRow } from './InputRow';
import { SweepSettings } from './SweepSettings';
import { SweepToolbar } from './SweepToolbar';

interface InputEditorProps {
  model: ModelInfo;
  entries: readonly TirEntry[];
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
  /** Sweep to select on mount, when the spec still has it (survives a remount). */
  initialSelectedId?: string;
  /** Called whenever the selected sweep changes. */
  onSelect?: (id: string) => void;
}

export function InputEditor({ model, entries, spec, onChange, initialSelectedId, onSelect }: InputEditorProps) {
  const [selectedId, selectSweep] = useState(
    spec.sweeps.some((sweep) => sweep.id === initialSelectedId) ? (initialSelectedId as string) : spec.sweeps[0].id,
  );
  const setSelectedId = (id: string) => {
    selectSweep(id);
    onSelect?.(id);
  };
  const index = sweepIndexOf(spec.sweeps, selectedId);

  const add = () => {
    const next = addSweep(spec, model, entries);
    if (next !== spec) {
      onChange(next);
      setSelectedId(next.sweeps[next.sweeps.length - 1].id);
    }
  };
  const duplicate = () => {
    const next = duplicateSweep(spec, index);
    if (next !== spec) {
      onChange(next);
      setSelectedId(next.sweeps[index + 1].id);
    }
  };
  const remove = () => {
    const next = removeSweep(spec, index);
    if (next !== spec) {
      onChange(next);
      setSelectedId(next.sweeps[Math.max(0, index - 1)].id);
    }
  };

  return (
    <div data-testid="input-editor" className="flex min-h-0 flex-col xl:flex-1">
      <div className="border-b px-3 py-2">
        <h2 className="text-[15px] font-semibold">Inputs</h2>
        <p className="text-xs text-muted-foreground">{`One input is swept; up to ${MAX_STEPS} are stepped.`}</p>
      </div>
      <Tabs
        value={spec.sweeps[index].id}
        onValueChange={setSelectedId}
        className="flex min-h-0 flex-col gap-0 xl:flex-1"
      >
        <SweepToolbar
          sweeps={spec.sweeps}
          onAdd={add}
          onDuplicate={duplicate}
          onRemove={remove}
        />
        {spec.sweeps.map((s, i) => (
          <TabsContent
            key={s.id}
            value={s.id}
            data-testid="sweep-editor"
            data-sweep={s.id}
            className="flex min-h-0 flex-col xl:flex-1"
          >
            <SweepSettings
              sweep={s}
              index={i}
              onRename={(name) => onChange(renameSweep(spec, i, name))}
              onNameBlur={() => onChange(freezeBlankName(spec, i))}
              onUncombined={(u) => onChange(replaceSweep(spec, i, { ...s, uncombined: u }))}
            />
            <ScrollArea className="xl:min-h-0 xl:flex-1">
              <div className="divide-y">
                {model.inputs.map((channel) => (
                  <InputRow
                    key={channel.name}
                    channel={channel}
                    entries={entries}
                    spec={s}
                    onChange={(next) => onChange(replaceSweep(spec, i, next))}
                  />
                ))}
              </div>
            </ScrollArea>
          </TabsContent>
        ))}
      </Tabs>
    </div>
  );
}
