import { useEffect, useMemo, useRef, useState } from 'react';
import { AppHeader } from '@/components/AppHeader';
import { EmptyState } from '@/components/EmptyState';
import { ChartView } from '@/components/chart/ChartView';
import { CoordinateSystemPanel } from '@/components/coordinates/CoordinateSystemPanel';
import { InputEditor } from '@/components/inputs/InputEditor';
import { ModelSummary } from '@/components/parameters/ModelSummary';
import { ParameterPanel } from '@/components/parameters/ParameterPanel';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Input } from '@/components/ui/input';
import type { ChartSpec } from '@/domain/chart-spec';
import { convertSpec, describeConversion } from '@/domain/coordinates';
import { defaultChartSpec } from '@/domain/defaults';
import { describeError } from '@/domain/errors';
import type { CoordinateSystem, TirEntry, TirInspection } from '@/domain/model';
import { channelsChanged, referencesMissing } from '@/domain/chart';
import { EXAMPLES } from '@/examples';
import { downloadText } from '@/lib/download';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { convertModel, inspectTir, loadSolver, setEntry } from '@/wasm/solver';

interface Document {
  fileName: string;
  text: string;
  original: TirEntry[];
  inspection: TirInspection;
  /** Identity of the loaded file; parameter edits keep it. */
  loadId: number;
}

export function App() {
  const [mf, setMf] = useState<MfWasmModule | null>(null);
  const [document, setDocument] = useState<Document | null>(null);
  const [spec, setSpec] = useState<ChartSpec | null>(null);
  // Bumped whenever the spec is replaced wholesale, so the input editor drops its local drafts.
  const [specEpoch, setSpecEpoch] = useState(0);
  const resetSpec = (next: ChartSpec | null) => {
    setSpec(next);
    setSpecEpoch((epoch) => epoch + 1);
  };
  const [conversionNote, setConversionNote] = useState<string | null>(null);
  const selectedSweep = useRef<string | undefined>(undefined);
  const loadCounter = useRef(0);
  const [error, setError] = useState<string | null>(null);
  const fileInput = useRef<HTMLInputElement>(null);

  useEffect(() => {
    loadSolver().then(setMf, (failure: unknown) => setError(`Solver failed to load: ${String(failure)}`));
  }, []);

  const open = (solver: MfWasmModule, fileName: string, text: string) => {
    const result = inspectTir(solver, text, fileName);
    if (!result.ok) {
      setError(`${fileName}: ${describeError(result.error)}`);
      return;
    }
    setError(null);
    loadCounter.current += 1;
    setDocument({
      fileName,
      text,
      original: result.value.entries,
      inspection: result.value,
      loadId: loadCounter.current,
    });
    setConversionNote(null);
    selectedSweep.current = undefined;
    resetSpec(
      result.value.model === null ? null : defaultChartSpec(result.value.model, result.value.entries, 'chart'),
    );
  };

  const onFile = async (file: File | undefined) => {
    if (file !== undefined && mf !== null) {
      // Keep a leading BOM so an unedited save is byte-identical; reject bytes that are not UTF-8.
      try {
        const decoder = new TextDecoder('utf-8', { ignoreBOM: true, fatal: true });
        open(mf, file.name, decoder.decode(await file.arrayBuffer()));
      } catch {
        setError('The file is not valid UTF-8 text.');
      }
    }
  };

  // Keep the spec across edits unless the edit changed the model's channels.
  const edit = (entry: TirEntry, value: string): string | null => {
    if (mf === null || document === null) {
      return 'Solver not ready.';
    }
    const replaced = setEntry(mf, document.text, entry.section, entry.key, value);
    if (!replaced.ok) {
      return describeError(replaced.error);
    }
    const inspected = inspectTir(mf, replaced.value, document.fileName);
    if (!inspected.ok) {
      return describeError(inspected.error);
    }
    setDocument({ ...document, text: replaced.value, inspection: inspected.value });
    const next = inspected.value.model;
    if (next === null) {
      resetSpec(null);
    } else if (spec === null || channelsChanged(document.inspection.model, next) || referencesMissing(spec, next)) {
      resetSpec(defaultChartSpec(next, inspected.value.entries, 'chart'));
    }
    return null;
  };

  const convert = (target: CoordinateSystem) => {
    if (mf === null || document === null) {
      return;
    }
    const converted = convertModel(mf, document.text, document.fileName, target);
    if (!converted.ok) {
      setError(`${document.fileName}: ${describeError(converted.error)}`);
      return;
    }
    const inspected = inspectTir(mf, converted.value.text, document.fileName);
    if (!inspected.ok) {
      setError(`${document.fileName}: ${describeError(inspected.error)}`);
      return;
    }
    setError(null);
    setDocument({ ...document, text: converted.value.text, inspection: inspected.value });
    const inputs = document.inspection.model?.inputs;
    if (spec !== null && inputs !== undefined) {
      const next = convertSpec(spec, inputs, converted.value.applied);
      if (next !== spec) {
        resetSpec(next);
      }
    }
    setConversionNote(describeConversion(target, converted.value));
  };

  const save = () => {
    if (document !== null) {
      downloadText(document.fileName, document.text);
    }
  };

  const model = document?.inspection.model ?? null;
  const loadError = document?.inspection.loadError ?? null;
  const warnings = useMemo(() => document?.inspection.warnings ?? [], [document]);

  const load = () => fileInput.current?.click();
  const openExample = (example: (typeof EXAMPLES)[number]) => {
    if (mf !== null) {
      open(mf, example.fileName, example.text);
    }
  };

  return (
    <div data-testid="app-shell" className="flex min-h-dvh w-full flex-col bg-background xl:h-dvh">
      <Input
        ref={fileInput}
        type="file"
        accept=".tir,text/plain"
        className="hidden"
        data-testid="file-input"
        onChange={(event) => {
          void onFile(event.target.files?.[0]);
          event.target.value = '';
        }}
      />
      <AppHeader
        solverReady={mf !== null}
        fileName={document?.fileName ?? null}
        model={model}
        canSave={document !== null}
        onLoad={load}
        onOpenExample={openExample}
        onSave={save}
      />

      {error !== null && (
        <Alert variant="destructive" data-testid="error-alert" className="rounded-none border-x-0 border-t-0">
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}

      {document === null ? (
        <EmptyState solverReady={mf !== null} onLoad={load} onOpenExample={openExample} />
      ) : (
        <main className="grid w-full flex-1 grid-cols-1 lg:grid-cols-[minmax(22rem,26rem)_minmax(0,1fr)] xl:min-h-0 xl:grid-cols-[minmax(22rem,26rem)_minmax(0,1fr)_minmax(24rem,30rem)]">
          <section
            data-testid="inputs-pane"
            aria-label="Inputs"
            className="flex flex-col border-b bg-card lg:border-r xl:min-h-0"
          >
            {model !== null && spec !== null ? (
              <InputEditor
                key={specEpoch}
                model={model}
                entries={document.inspection.entries}
                spec={spec}
                onChange={setSpec}
                initialSelectedId={selectedSweep.current}
                onSelect={(id) => {
                  selectedSweep.current = id;
                }}
              />
            ) : (
              <p className="p-3 text-sm text-muted-foreground">
                No model inputs: fix the file in the tire model panel.
              </p>
            )}
          </section>
          <section data-testid="chart-pane" aria-label="Plot" className="flex min-w-0 flex-col border-b bg-card xl:min-h-0">
            {loadError !== null && (
              <Alert variant="destructive" data-testid="load-error" className="m-3 w-auto">
                <AlertTitle>The model cannot be built from this file</AlertTitle>
                <AlertDescription>{describeError(loadError)}</AlertDescription>
              </Alert>
            )}
            {mf !== null && model !== null && spec !== null && (
              <ChartView mf={mf} text={document.text} model={model} spec={spec} onChange={setSpec} />
            )}
          </section>
          <aside
            data-testid="model-pane"
            aria-label="Tire model"
            className="flex min-w-0 flex-col bg-card lg:col-span-2 xl:col-span-1 xl:min-h-0 xl:overflow-y-auto xl:border-l"
          >
            <ModelSummary model={model} warnings={warnings} />
            {model !== null && (
              <CoordinateSystemPanel
                key={document.loadId}
                identification={document.inspection.coordinates}
                conversionNote={conversionNote}
                onConvert={convert}
              />
            )}
            <ParameterPanel original={document.original} entries={document.inspection.entries} onEdit={edit} />
          </aside>
        </main>
      )}
    </div>
  );
}
