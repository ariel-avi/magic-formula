import { useMemo } from 'react';
import { useTheme } from '@/components/ThemeProvider';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Label } from '@/components/ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Switch } from '@/components/ui/switch';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { channelLabel } from '@/domain/channels';
import type { ChartSpec } from '@/domain/chart-spec';
import type { ModelInfo } from '@/domain/model';
import { buildChartData } from '@/domain/series';
import { displayUnit, hasUnit } from '@/domain/units';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { SeriesLegend } from './SeriesLegend';
import { SeriesTable } from './SeriesTable';
import { SweepChart } from './SweepChart';
import { useSweepSolution } from './useSweepSolution';

interface ChartViewProps {
  mf: MfWasmModule;
  text: string;
  model: ModelInfo;
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
}

export function ChartView({ mf, text, model, spec, onChange }: ChartViewProps) {
  const { theme } = useTheme();
  const { problems, error, solution } = useSweepSolution(mf, text, model, spec);
  const data = useMemo(
    () =>
      solution === null
        ? null
        : buildChartData(spec, solution.batch, solution.outputs, model.inputs, model.outputs, theme),
    [solution, spec, model, theme],
  );

  return (
    <Tabs defaultValue="plot" data-testid="chart-view" className="flex min-h-0 flex-1 flex-col gap-0">
      <div className="flex flex-wrap items-center gap-x-3 gap-y-2 border-b px-3 py-2">
        <Select value={spec.output} onValueChange={(output) => onChange({ ...spec, output })}>
          <SelectTrigger size="sm" aria-label="Output" data-testid="output-select" className="min-w-36 font-mono text-xs">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {model.outputs.map((c) => {
              const unit = displayUnit(c.unit).label;
              return (
                <SelectItem
                  key={c.name}
                  value={c.name}
                  description={channelLabel(c.name).description}
                  className="font-mono text-xs"
                >
                  {hasUnit(unit) ? `${c.name} [${unit}]` : c.name}
                </SelectItem>
              );
            })}
          </SelectContent>
        </Select>
        <span className="text-xs text-muted-foreground">vs {data === null ? spec.sweep.variable : data.xLabel}</span>
        <div className="flex items-center gap-2">
          <Switch
            id="uncombined"
            data-testid="uncombined-switch"
            checked={spec.uncombined}
            onCheckedChange={(uncombined) => onChange({ ...spec, uncombined })}
          />
          <Label htmlFor="uncombined" className="text-xs">
            Pure slip (uncombined)
          </Label>
        </div>
        <TabsList className="ml-auto h-7">
          <TabsTrigger value="plot" className="text-xs">
            Plot
          </TabsTrigger>
          <TabsTrigger value="table" className="text-xs">
            Table
          </TabsTrigger>
        </TabsList>
      </div>
      {problems.length > 0 && (
        <Alert variant="destructive" data-testid="chart-problems" className="mx-3 mt-3 w-auto">
          <AlertTitle>The chart settings are not valid</AlertTitle>
          <AlertDescription>
            <ul className="list-disc pl-4">
              {problems.map((problem) => (
                <li key={problem}>{problem}</li>
              ))}
            </ul>
          </AlertDescription>
        </Alert>
      )}
      {error !== null && (
        <Alert variant="destructive" data-testid="chart-error" className="mx-3 mt-3 w-auto">
          <AlertTitle>The solver failed</AlertTitle>
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}
      {data !== null && (
        <>
          <TabsContent value="plot" className="h-[26rem] min-h-0 p-3 xl:h-auto xl:flex-1">
            <SweepChart data={data} />
          </TabsContent>
          <TabsContent value="table" className="h-[26rem] min-h-0 p-3 xl:h-auto xl:flex-1">
            <SeriesTable data={data} />
          </TabsContent>
          <SeriesLegend series={data.series} />
        </>
      )}
    </Tabs>
  );
}
