import { useEffect, useMemo, useRef, useState } from 'react';
import { ChartPanel } from '@/components/ChartPanel';
import { ParameterEditor } from '@/components/ParameterEditor';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import type { ChartSpec } from '@/domain/chart-spec';
import { defaultChartSpec } from '@/domain/defaults';
import type { MfError, TirEntry, TirInspection } from '@/domain/model';
import { channelsChanged, referencesMissing } from '@/domain/steps';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { inspectTir, loadSolver, setEntry } from '@/wasm/solver';
import exampleMf52 from '../../tests/data/mf52_S52.tir?raw';
import examplePac2002 from '../../tests/data/pac2002_S.tir?raw';

interface Document {
  fileName: string;
  text: string;
  original: TirEntry[];
  inspection: TirInspection;
}

const EXAMPLES = [
  { fileName: 'pac2002_S.tir', text: examplePac2002 },
  { fileName: 'mf52_S52.tir', text: exampleMf52 },
];

const describe = (error: MfError) => `${error.message}${error.line > 0 ? ` (line ${error.line})` : ''}`;

export function App() {
  const [mf, setMf] = useState<MfWasmModule | null>(null);
  const [document, setDocument] = useState<Document | null>(null);
  const [spec, setSpec] = useState<ChartSpec | null>(null);
  // Bumped whenever the spec is replaced wholesale, so ChartPanel drops its local text drafts.
  const [specEpoch, setSpecEpoch] = useState(0);
  const resetSpec = (next: ChartSpec | null) => {
    setSpec(next);
    setSpecEpoch((epoch) => epoch + 1);
  };
  const [error, setError] = useState<string | null>(null);
  const fileInput = useRef<HTMLInputElement>(null);

  useEffect(() => {
    loadSolver().then(setMf, (failure: unknown) => setError(`Solver failed to load: ${String(failure)}`));
  }, []);

  const open = (solver: MfWasmModule, fileName: string, text: string) => {
    const result = inspectTir(solver, text, fileName);
    if (!result.ok) {
      setError(`${fileName}: ${describe(result.error)}`);
      return;
    }
    setError(null);
    setDocument({ fileName, text, original: result.value.entries, inspection: result.value });
    resetSpec(result.value.model === null ? null : defaultChartSpec(result.value.model, result.value.entries, 'chart'));
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
      return describe(replaced.error);
    }
    const inspected = inspectTir(mf, replaced.value, document.fileName);
    if (!inspected.ok) {
      return describe(inspected.error);
    }
    setDocument({ ...document, text: replaced.value, inspection: inspected.value });
    const model = inspected.value.model;
    if (model === null) {
      resetSpec(null);
    } else if (spec === null || channelsChanged(document.inspection.model, model) || referencesMissing(spec, model)) {
      resetSpec(defaultChartSpec(model, inspected.value.entries, 'chart'));
    }
    return null;
  };

  const save = () => {
    if (document === null) {
      return;
    }
    const url = URL.createObjectURL(new Blob([document.text], { type: 'text/plain' }));
    const link = window.document.createElement('a');
    link.href = url;
    link.download = document.fileName;
    window.document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };

  const model = document?.inspection.model ?? null;
  const warnings = useMemo(() => document?.inspection.warnings ?? [], [document]);

  return (
    <div className="mx-auto max-w-7xl space-y-4 p-4">
      <header className="flex flex-wrap items-center gap-3">
        <h1 className="text-xl font-semibold">Magic Formula Workbench</h1>
        {model !== null && (
          <Badge data-testid="model-name">
            {model.name}
            {model.fittyp !== null && ` (FITTYP ${model.fittyp})`}
          </Badge>
        )}
        {document !== null && <span className="text-sm text-muted-foreground">{document.fileName}</span>}
        <div className="ml-auto flex gap-2">
          <input
            ref={fileInput}
            type="file"
            accept=".tir,text/plain"
            className="hidden"
            onChange={(event) => {
              void onFile(event.target.files?.[0]);
              event.target.value = '';
            }}
          />
          <Button disabled={mf === null} onClick={() => fileInput.current?.click()}>
            Load .tir
          </Button>
          <Button variant="outline" disabled={document === null} onClick={save}>
            Save .tir
          </Button>
        </div>
      </header>

      {error !== null && (
        <Alert variant="destructive">
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}

      {document === null ? (
        <Card>
          <CardHeader>
            <CardTitle>{mf === null ? 'Loading solver...' : 'Load a tire property file'}</CardTitle>
          </CardHeader>
          <CardContent className="flex flex-wrap gap-2">
            {EXAMPLES.map((example) => (
              <Button
                key={example.fileName}
                variant="secondary"
                disabled={mf === null}
                onClick={() => mf !== null && open(mf, example.fileName, example.text)}
              >
                Open example {example.fileName}
              </Button>
            ))}
          </CardContent>
        </Card>
      ) : (
        <>
          {document.inspection.loadError !== null && (
            <Alert variant="destructive">
              <AlertTitle>The model cannot be built from this file</AlertTitle>
              <AlertDescription>{describe(document.inspection.loadError)}</AlertDescription>
            </Alert>
          )}
          <Tabs defaultValue="charts">
            <TabsList>
              <TabsTrigger value="charts">Charts</TabsTrigger>
              <TabsTrigger value="parameters">Parameters</TabsTrigger>
              <TabsTrigger value="model">Model</TabsTrigger>
            </TabsList>
            <TabsContent value="charts">
              {mf !== null && model !== null && spec !== null ? (
                <ChartPanel
                  key={specEpoch}
                  mf={mf}
                  text={document.text}
                  model={model}
                  entries={document.inspection.entries}
                  spec={spec}
                  onChange={setSpec}
                />
              ) : (
                <p className="text-sm text-muted-foreground">No model: fix the file in the Parameters tab.</p>
              )}
            </TabsContent>
            <TabsContent value="parameters">
              <ParameterEditor original={document.original} entries={document.inspection.entries} onEdit={edit} />
            </TabsContent>
            <TabsContent value="model">
              <Card>
                <CardContent className="space-y-3 pt-6 text-sm">
                  {model === null ? (
                    <p>No model.</p>
                  ) : (
                    <>
                      <p>
                        <strong>{model.name}</strong>
                        {model.fittyp !== null && `, FITTYP ${model.fittyp}`}
                        {model.useMode !== null && `, USE_MODE ${model.useMode}`}, tyre side {model.tyreSide}
                      </p>
                      <p>Inputs: {model.inputs.map((c) => `${c.name} [${c.unit}]`).join(', ')}</p>
                      <p>Outputs: {model.outputs.map((c) => `${c.name} [${c.unit}]`).join(', ')}</p>
                      <p>
                        Limits:{' '}
                        {model.limits.map((limit) => `${limit.key} = ${limit.value}`).join(', ') || 'none'}
                      </p>
                    </>
                  )}
                  {warnings.length > 0 && (
                    <div>
                      <h3 className="font-semibold">Warnings</h3>
                      <ul className="list-disc pl-5">
                        {warnings.map((warning, index) => (
                          <li key={index}>
                            {warning.message}
                            {warning.line > 0 && ` (line ${warning.line})`}
                          </li>
                        ))}
                      </ul>
                    </div>
                  )}
                </CardContent>
              </Card>
            </TabsContent>
          </Tabs>
        </>
      )}
    </div>
  );
}
