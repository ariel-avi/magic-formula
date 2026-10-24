import { useMemo, useState } from 'react';
import { CartesianGrid, Legend, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import { Alert, AlertDescription } from '@/components/ui/alert';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { buildBatch } from '@/domain/batch';
import { MAX_STEPS, type ChartSpec, type ColourMode } from '@/domain/chart-spec';
import type { ModelInfo, TirEntry } from '@/domain/model';
import { buildChartData } from '@/domain/series';
import { addStep, availableStepVariables, removeStep, setColourMode, validateChartSpec } from '@/domain/steps';
import { defaultSweep } from '@/domain/defaults';
import { parseValueList } from '@/domain/sweep';
import { displayUnit } from '@/domain/units';
import { solveBatch } from '@/wasm/solver';
import type { MfWasmModule } from '@/wasm/mf_wasm.js';
import { Button } from '@/components/ui/button';

const SELECT_CLASS = 'h-9 rounded-md border bg-background px-2 text-sm';

interface Props {
  mf: MfWasmModule;
  text: string;
  model: ModelInfo;
  entries: readonly TirEntry[];
  spec: ChartSpec;
  onChange: (spec: ChartSpec) => void;
}

const shown = (value: number) => (Number.isFinite(value) ? value : '');

function numberOr(text: string, fallback: number): number {
  const value = Number(text);
  return text.trim() !== '' && Number.isFinite(value) ? value : fallback;
}

export function ChartPanel({ mf, text, model, entries, spec, onChange }: Props) {
  const problems = useMemo(() => validateChartSpec(spec, model.inputs, model.outputs), [spec, model]);

  const outcome = useMemo(() => {
    if (problems.length > 0) {
      return null;
    }
    const batch = buildBatch(spec, model.inputs);
    const result = solveBatch(mf, text, batch, spec.uncombined);
    if (!result.ok) {
      const row = result.error.row;
      let where = '';
      if (row !== null && row !== undefined) {
        const points = spec.sweep.points;
        const combination = batch.combinations[Math.floor(row / points)];
        const steps = spec.steps.map((step, k) => `${step.variable} = ${combination?.values[k]}`);
        const swept = `${spec.sweep.variable} = ${batch.grid[row % points]}`;
        where = ` (${[...steps, swept].join(', ')})`;
      }
      return { error: `${result.error.message}${where}`, data: null };
    }
    return { error: null, data: buildChartData(spec, batch, result.value, model.inputs, model.outputs) };
  }, [mf, text, model, spec, problems]);

  const unitLabel = (name: string) => {
    const channel = model.inputs.find((input) => input.name === name);
    return channel === undefined ? '' : displayUnit(channel.unit).label;
  };
  const stepNames = new Set(spec.steps.map((step) => step.variable));

  const changeSweepVariable = (name: string) => {
    const channel = model.inputs.find((input) => input.name === name)!;
    onChange({
      ...spec,
      sweep: defaultSweep(channel, entries),
      steps: spec.steps.filter((step) => step.variable !== name),
    });
  };

  const addNewStep = () => {
    const variable = availableStepVariables(spec, model.inputs, spec.steps.length)[0];
    if (variable !== undefined) {
      onChange({ ...spec, steps: addStep(spec.steps, variable, [spec.fixed[variable]]) });
    }
  };

  return (
    <div className="grid gap-4 lg:grid-cols-[22rem_1fr]">
      <Card>
        <CardHeader>
          <CardTitle>Sweep</CardTitle>
        </CardHeader>
        <CardContent className="space-y-3">
          <div className="grid grid-cols-2 gap-2">
            <div className="space-y-1">
              <Label>Sweep variable</Label>
              <select
                className={SELECT_CLASS}
                value={spec.sweep.variable}
                onChange={(event) => changeSweepVariable(event.target.value)}
              >
                {model.inputs.map((channel) => (
                  <option key={channel.name}>{channel.name}</option>
                ))}
              </select>
            </div>
            <div className="space-y-1">
              <Label>Output</Label>
              <select
                className={SELECT_CLASS}
                value={spec.output}
                onChange={(event) => onChange({ ...spec, output: event.target.value })}
              >
                {model.outputs.map((channel) => (
                  <option key={channel.name}>{channel.name}</option>
                ))}
              </select>
            </div>
          </div>
          <div className="grid grid-cols-3 gap-2">
            {(['start', 'end', 'points'] as const).map((field) => (
              <div key={field} className="space-y-1">
                <Label>
                  {field}
                  {field !== 'points' && unitLabel(spec.sweep.variable) ? ` [${unitLabel(spec.sweep.variable)}]` : ''}
                </Label>
                <Input
                  type="number"
                  value={shown(spec.sweep[field])}
                  onChange={(event) =>
                    onChange({ ...spec, sweep: { ...spec.sweep, [field]: numberOr(event.target.value, NaN) } })
                  }
                />
              </div>
            ))}
          </div>
          <div className="flex items-center gap-2">
            <Switch
              checked={spec.uncombined}
              onCheckedChange={(checked) => onChange({ ...spec, uncombined: checked })}
              id="uncombined"
            />
            <Label htmlFor="uncombined">Pure slip (uncombined)</Label>
          </div>

          <div className="flex items-center justify-between pt-2">
            <Label>Steps (max {MAX_STEPS})</Label>
            <Button size="sm" variant="outline" disabled={spec.steps.length >= MAX_STEPS} onClick={addNewStep}>
              Add step
            </Button>
          </div>
          {spec.steps.map((step, k) => (
            <div key={k} className="space-y-2 rounded-md border p-2">
              <div className="flex gap-2">
                <select
                  className={SELECT_CLASS}
                  value={step.variable}
                  onChange={(event) => {
                    const variable = event.target.value;
                    onChange({
                      ...spec,
                      steps: spec.steps.map((s, i) =>
                        i === k ? { ...s, variable, values: [spec.fixed[variable]] } : s,
                      ),
                    });
                  }}
                >
                  {[...new Set([step.variable, ...availableStepVariables(spec, model.inputs, k)])].map((name) => (
                    <option key={name}>{name}</option>
                  ))}
                </select>
                <select
                  className={SELECT_CLASS}
                  value={step.colourMode}
                  onChange={(event) =>
                    onChange({ ...spec, steps: setColourMode(spec.steps, k, event.target.value as ColourMode) })
                  }
                >
                  <option value="hue">Hue</option>
                  <option value="saturation">Saturation</option>
                </select>
                <Button size="sm" variant="ghost" onClick={() => onChange({ ...spec, steps: removeStep(spec.steps, k) })}>
                  Remove
                </Button>
              </div>
              <StepValuesInput
                key={`${step.variable}-${k}`}
                label={`Values${unitLabel(step.variable) ? ` [${unitLabel(step.variable)}]` : ''}`}
                initial={step.values.join(', ')}
                onValues={(values) =>
                  onChange({ ...spec, steps: spec.steps.map((s, i) => (i === k ? { ...s, values } : s)) })
                }
              />
            </div>
          ))}

          <Label className="block pt-2">Fixed values</Label>
          <div className="grid grid-cols-2 gap-2">
            {model.inputs
              .filter((channel) => channel.name !== spec.sweep.variable && !stepNames.has(channel.name))
              .map((channel) => (
                <div key={channel.name} className="space-y-1">
                  <Label>
                    {channel.name} {displayUnit(channel.unit).label && `[${displayUnit(channel.unit).label}]`}
                  </Label>
                  <Input
                    type="number"
                    value={shown(spec.fixed[channel.name])}
                    onChange={(event) =>
                      onChange({ ...spec, fixed: { ...spec.fixed, [channel.name]: numberOr(event.target.value, NaN) } })
                    }
                  />
                </div>
              ))}
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardContent className="pt-6">
          {problems.length > 0 && (
            <Alert variant="destructive">
              <AlertDescription>{problems.join(' ')}</AlertDescription>
            </Alert>
          )}
          {outcome?.error != null && (
            <Alert variant="destructive">
              <AlertDescription>{outcome.error}</AlertDescription>
            </Alert>
          )}
          {outcome?.data != null && (
            <div className="h-[32rem] w-full">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={outcome.data.rows} margin={{ top: 8, right: 16, bottom: 24, left: 16 }}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis
                    dataKey="x"
                    type="number"
                    domain={['dataMin', 'dataMax']}
                    label={{ value: outcome.data.xLabel, position: 'insideBottom', offset: -12 }}
                    tickFormatter={(value: number) => String(Number(value.toPrecision(4)))}
                  />
                  <YAxis
                    label={{ value: outcome.data.yLabel, angle: -90, position: 'insideLeft' }}
                    tickFormatter={(value: number) => String(Number(value.toPrecision(4)))}
                  />
                  <Tooltip formatter={(value) => Number(value).toPrecision(5)} labelFormatter={(x) => `${outcome.data.xLabel} = ${Number(x).toPrecision(5)}`} />
                  <Legend verticalAlign="bottom" wrapperStyle={{ paddingTop: 16 }} />
                  {outcome.data.series.map((series) => (
                    <Line
                      key={series.key}
                      dataKey={series.key}
                      name={series.label}
                      stroke={series.colour}
                      dot={false}
                      strokeWidth={2}
                      isAnimationActive={false}
                    />
                  ))}
                </LineChart>
              </ResponsiveContainer>
            </div>
          )}
        </CardContent>
      </Card>
    </div>
  );
}

function StepValuesInput({
  label,
  initial,
  onValues,
}: {
  label: string;
  initial: string;
  onValues: (values: number[]) => void;
}) {
  const [draft, setDraft] = useState(initial);
  const parsed = parseValueList(draft);
  return (
    <div className="space-y-1">
      <Label>{label}</Label>
      <Input
        value={draft}
        aria-invalid={!parsed.ok}
        onChange={(event) => {
          setDraft(event.target.value);
          const result = parseValueList(event.target.value);
          if (result.ok) {
            onValues(result.value);
          }
        }}
      />
      {!parsed.ok && <p className="text-xs text-destructive">{parsed.error}</p>}
    </div>
  );
}
