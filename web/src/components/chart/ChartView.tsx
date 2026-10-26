import { useMemo } from 'react';
import { useTheme } from '@/components/ThemeProvider';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { axisOptions } from '@/domain/axes';
import { sweepDisplayName, validateAxes } from '@/domain/chart';
import type { ChartSpec } from '@/domain/chart-spec';
import type { ModelInfo } from '@/domain/model';
import { buildChartData, flatXGroups } from '@/domain/series';
import { solvedSweeps } from '@/domain/solve';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { AxisSelect } from './AxisSelect';
import { SeriesLegend } from './SeriesLegend';
import { SeriesTable } from './SeriesTable';
import { SweepChart } from './SweepChart';
import { useSweepSolutions } from './useSweepSolutions';

interface ChartViewProps {
  mf: MfWasmModule;
  text: string;
  model: ModelInfo;
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
}

export function ChartView({ mf, text, model, spec, onChange }: ChartViewProps) {
  const { theme } = useTheme();
  const outcomes = useSweepSolutions(mf, text, model, spec.sweeps);
  const options = useMemo(() => axisOptions(model), [model]);
  const axisProblems = useMemo(() => validateAxes(spec, model), [spec, model]);
  const data = useMemo(() => {
    if (axisProblems.length > 0) return null;
    const solved = solvedSweeps(spec.sweeps, outcomes);
    return solved.length === 0 ? null : buildChartData(spec, solved, model, theme);
  }, [spec, outcomes, model, theme, axisProblems]);
  const flat = useMemo(() => (data === null ? [] : flatXGroups(data.groups)), [data]);
  const grouped = spec.sweeps.length > 1;

  return (
    <Tabs defaultValue="plot" data-testid="chart-view" className="flex min-h-0 flex-1 flex-col gap-0">
      <div className="flex flex-wrap items-center gap-x-3 gap-y-2 border-b px-3 py-2">
        <AxisSelect axis="y" options={options} value={spec.y} onChange={(y) => onChange({ ...spec, y })} />
        <span className="text-xs text-muted-foreground">vs</span>
        <AxisSelect axis="x" options={options} value={spec.x} onChange={(x) => onChange({ ...spec, x })} />
        <TabsList className="ml-auto h-7">
          <TabsTrigger value="plot" className="text-xs">
            Plot
          </TabsTrigger>
          <TabsTrigger value="table" className="text-xs">
            Table
          </TabsTrigger>
        </TabsList>
      </div>
      {axisProblems.length > 0 && (
        <Alert variant="destructive" data-testid="axis-problems" className="mx-3 mt-3 w-auto">
          <AlertTitle className="line-clamp-none">The axis settings are not valid</AlertTitle>
          <AlertDescription>
            <ul className="list-disc pl-4">
              {axisProblems.map((problem) => (
                <li key={problem}>{problem}</li>
              ))}
            </ul>
          </AlertDescription>
        </Alert>
      )}
      {spec.sweeps.map((sweep, i) =>
        outcomes[i].problems.length > 0 ? (
          <Alert
            key={`problems-${sweep.id}`}
            variant="destructive"
            data-testid="chart-problems"
            data-sweep={sweep.id}
            className="mx-3 mt-3 w-auto"
          >
            <AlertTitle className="line-clamp-none">
              {grouped
                ? `${sweepDisplayName(sweep, i)}: the sweep settings are not valid`
                : 'The chart settings are not valid'}
            </AlertTitle>
            <AlertDescription>
              <ul className="list-disc pl-4">
                {outcomes[i].problems.map((problem) => (
                  <li key={problem}>{problem}</li>
                ))}
              </ul>
            </AlertDescription>
          </Alert>
        ) : null,
      )}
      {spec.sweeps.map((sweep, i) =>
        outcomes[i].error !== null ? (
          <Alert
            key={`error-${sweep.id}`}
            variant="destructive"
            data-testid="chart-error"
            data-sweep={sweep.id}
            className="mx-3 mt-3 w-auto"
          >
            <AlertTitle className="line-clamp-none">
              {grouped ? `${sweepDisplayName(sweep, i)}: the solver failed` : 'The solver failed'}
            </AlertTitle>
            <AlertDescription>{outcomes[i].error}</AlertDescription>
          </Alert>
        ) : null,
      )}
      {spec.sweeps.map((sweep, i) =>
        outcomes[i].warnings.length > 0 ? (
          <Alert
            key={`warnings-${sweep.id}`}
            data-testid="chart-warnings"
            data-sweep={sweep.id}
            className="mx-3 mt-3 w-auto"
          >
            <AlertTitle className="line-clamp-none">
              {grouped ? `${sweepDisplayName(sweep, i)}: solver warnings` : 'Solver warnings'}
            </AlertTitle>
            <AlertDescription>
              <ul className="list-disc pl-4">
                {outcomes[i].warnings.map((warning) => (
                  <li key={warning}>{warning}</li>
                ))}
              </ul>
            </AlertDescription>
          </Alert>
        ) : null,
      )}
      {data !== null && (
        <>
          {flat.length > 0 && (
            <p data-testid="flat-sweeps" className="px-3 pt-2 text-xs text-muted-foreground">
              {flat.map((g) => `${g.name} does not vary along ${data.xLabel}; choose X = ${g.sweptLabel} to see it.`).join(' ')}
            </p>
          )}
          <TabsContent value="plot" className="h-[26rem] min-h-0 p-3 xl:h-auto xl:flex-1">
            <SweepChart data={data} />
          </TabsContent>
          <TabsContent value="table" className="h-[26rem] min-h-0 p-3 xl:h-auto xl:flex-1">
            <SeriesTable data={data} grouped={grouped} />
          </TabsContent>
          <SeriesLegend data={data} grouped={grouped} />
        </>
      )}
    </Tabs>
  );
}
