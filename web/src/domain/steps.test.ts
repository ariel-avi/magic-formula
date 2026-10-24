import { describe, expect, it } from 'vitest';
import { MAX_STEP_VALUES, type ChartSpec, type StepSpec } from './chart-spec';
import type { InputChannel, ModelInfo, OutputChannel } from './model';
import {
  addStep,
  availableStepVariables,
  channelsChanged,
  referencesMissing,
  removeStep,
  setColourMode,
  stepCombinations,
  validateChartSpec,
} from './steps';

const inputs: InputChannel[] = ['FZ', 'SA', 'SR', 'IA', 'VX'].map((name) => ({
  name,
  unit: '-',
  minimum: 0,
  minimumInclusive: true,
}));
const outputs: OutputChannel[] = [
  { name: 'FY', unit: 'N' },
  { name: 'FX', unit: 'N' },
];

const step = (variable: string, values: number[], colourMode: StepSpec['colourMode']): StepSpec => ({
  variable,
  values,
  colourMode,
});

function makeSpec(overrides: Partial<ChartSpec> = {}): ChartSpec {
  return {
    id: 'c1',
    sweep: { variable: 'SA', start: -10, end: 10, points: 21 },
    steps: [step('FZ', [3000, 4000], 'hue')],
    fixed: { SR: 0, IA: 0, VX: 16.7 },
    output: 'FY',
    uncombined: false,
    ...overrides,
  };
}

describe('stepCombinations', () => {
  it('gives one empty combination without steps', () => {
    expect(stepCombinations([])).toEqual([{ indices: [], values: [] }]);
  });

  it('lists one combination per value for one step', () => {
    const combos = stepCombinations([step('FZ', [1, 2, 3], 'hue')]);
    expect(combos.map((c) => c.values)).toEqual([[1], [2], [3]]);
    expect(combos.map((c) => c.indices)).toEqual([[0], [1], [2]]);
  });

  it('builds the cartesian product with step 0 outermost', () => {
    const combos = stepCombinations([step('FZ', [10, 20], 'hue'), step('IA', [1, 2, 3], 'saturation')]);
    expect(combos).toHaveLength(6);
    expect(combos.map((c) => c.indices)).toEqual([
      [0, 0],
      [0, 1],
      [0, 2],
      [1, 0],
      [1, 1],
      [1, 2],
    ]);
    expect(combos[4].values).toEqual([20, 2]);
  });
});

describe('addStep', () => {
  it('gives the first step the hue mode', () => {
    expect(addStep([], 'FZ', [1])).toEqual([step('FZ', [1], 'hue')]);
  });

  it('gives the second step the complement of the first mode', () => {
    expect(addStep([step('FZ', [1], 'hue')], 'IA', [2])[1].colourMode).toBe('saturation');
    expect(addStep([step('FZ', [1], 'saturation')], 'IA', [2])[1].colourMode).toBe('hue');
  });

  it('ignores a third step', () => {
    const two = [step('FZ', [1], 'hue'), step('IA', [2], 'saturation')];
    const result = addStep(two, 'SR', [3]);
    expect(result).toEqual(two);
    expect(result).not.toBe(two);
  });
});

describe('removeStep', () => {
  it('removes the step at the index', () => {
    const two = [step('FZ', [1], 'hue'), step('IA', [2], 'saturation')];
    expect(removeStep(two, 0)).toEqual([two[1]]);
  });
});

describe('setColourMode', () => {
  it('sets the mode of a single step', () => {
    expect(setColourMode([step('FZ', [1], 'hue')], 0, 'saturation')).toEqual([step('FZ', [1], 'saturation')]);
  });

  it('leaves a single step unchanged when the index matches no step', () => {
    expect(setColourMode([step('FZ', [1], 'hue')], 1, 'saturation')).toEqual([step('FZ', [1], 'hue')]);
  });

  it('flips the other step to the complement when there are two', () => {
    const two = [step('FZ', [1], 'hue'), step('IA', [2], 'saturation')];
    const result = setColourMode(two, 1, 'hue');
    expect(result.map((s) => s.colourMode)).toEqual(['saturation', 'hue']);
  });
});

describe('availableStepVariables', () => {
  const spec = makeSpec({ steps: [step('FZ', [1], 'hue'), step('IA', [2], 'saturation')] });

  it('excludes the sweep variable and the other steps variable', () => {
    expect(availableStepVariables(spec, inputs, 0)).toEqual(['FZ', 'SR', 'VX']);
    expect(availableStepVariables(spec, inputs, 1)).toEqual(['SR', 'IA', 'VX']);
  });

  it('allows a new step index beyond the existing ones', () => {
    expect(availableStepVariables(spec, inputs, 2)).toEqual(['SR', 'VX']);
  });
});

describe('validateChartSpec', () => {
  const validate = (spec: ChartSpec) => validateChartSpec(spec, inputs, outputs);

  it('accepts a valid spec', () => {
    expect(validate(makeSpec())).toEqual([]);
  });

  it('accepts a spec without steps', () => {
    expect(validate(makeSpec({ steps: [], fixed: { FZ: 4000, SR: 0, IA: 0, VX: 16.7 } }))).toEqual([]);
  });

  it('reports an unknown sweep variable', () => {
    const spec = makeSpec({ sweep: { variable: 'XX', start: 0, end: 1, points: 5 } });
    expect(validate(spec)).toContain("Unknown sweep variable 'XX'.");
  });

  it.each([1, 502, 2.5])('reports %s sweep points', (points) => {
    const spec = makeSpec({ sweep: { variable: 'SA', start: 0, end: 1, points } });
    expect(validate(spec)).toEqual(['Sweep points must be an integer from 2 to 501.']);
  });

  it.each([
    [5, 5],
    [Number.NaN, 5],
    [0, Number.POSITIVE_INFINITY],
  ])('reports start %s and end %s', (start, end) => {
    const spec = makeSpec({ sweep: { variable: 'SA', start, end, points: 5 } });
    expect(validate(spec)).toEqual(['Sweep start and end must be finite and different.']);
  });

  it('reports more than two steps', () => {
    const spec = makeSpec({
      steps: [step('FZ', [1], 'hue'), step('IA', [1], 'saturation'), step('SR', [1], 'hue')],
      fixed: { VX: 16.7 },
    });
    expect(validate(spec)).toEqual(['At most 2 step variables are allowed.']);
  });

  it('reports an unknown step variable', () => {
    const spec = makeSpec({ steps: [step('XX', [1], 'hue')], fixed: { FZ: 1, SR: 0, IA: 0, VX: 1 } });
    expect(validate(spec)).toEqual(["Unknown step variable 'XX'."]);
  });

  it('reports a step on the sweep variable', () => {
    const spec = makeSpec({ steps: [step('SA', [1], 'hue')], fixed: { FZ: 1, SR: 0, IA: 0, VX: 1 } });
    expect(validate(spec)).toEqual(["Step variable 'SA' is the sweep variable."]);
  });

  it('reports a duplicate step variable', () => {
    const spec = makeSpec({ steps: [step('FZ', [1], 'hue'), step('FZ', [2], 'saturation')] });
    expect(validate(spec)).toEqual(["Step variable 'FZ' is used twice."]);
  });

  it.each([[[]], [Array.from({ length: MAX_STEP_VALUES + 1 }, (_, i) => i)]])('reports a step with %j values', (values) => {
    const spec = makeSpec({ steps: [step('FZ', values, 'hue')] });
    expect(validate(spec)).toEqual([`Step 'FZ' needs 1 to ${MAX_STEP_VALUES} values.`]);
  });

  it('reports a non-finite step value', () => {
    const spec = makeSpec({ steps: [step('FZ', [1, Number.NaN], 'hue')] });
    expect(validate(spec)).toEqual(["Step 'FZ' has a value that is not a finite number."]);
  });

  it('reports equal colour modes', () => {
    const spec = makeSpec({ steps: [step('FZ', [1], 'hue'), step('IA', [1], 'hue')], fixed: { SR: 0, VX: 1 } });
    expect(validate(spec)).toEqual(['The two steps must use different colour modes.']);
  });

  it('reports a missing fixed value', () => {
    const spec = makeSpec({ fixed: { SR: 0, IA: 0 } });
    expect(validate(spec)).toEqual(["Fixed value of 'VX' must be a finite number."]);
  });

  it('reports a non-finite fixed value', () => {
    const spec = makeSpec({ fixed: { SR: Number.NaN, IA: 0, VX: 1 } });
    expect(validate(spec)).toEqual(["Fixed value of 'SR' must be a finite number."]);
  });

  it('reports an unknown output', () => {
    expect(validate(makeSpec({ output: 'ZZ' }))).toEqual(["Unknown output 'ZZ'."]);
  });
});

describe('channelsChanged and referencesMissing', () => {
  const model = (inNames: string[], outNames: string[]): ModelInfo => ({
    name: 'm',
    fittyp: null,
    useMode: null,
    tyreSide: 'LEFT',
    inputs: inNames.map((name) => ({ name, unit: '-', minimum: 0, minimumInclusive: true })),
    outputs: outNames.map((name) => ({ name, unit: 'N' })),
    limits: [],
  });
  const base = model(['FZ', 'SA'], ['FY', 'FX']);

  it('treats a missing previous model as changed', () => {
    expect(channelsChanged(null, base)).toBe(true);
  });

  it('is false for identical channels', () => {
    expect(channelsChanged(base, model(['FZ', 'SA'], ['FY', 'FX']))).toBe(false);
  });

  it('detects input and output differences', () => {
    expect(channelsChanged(base, model(['FZ'], ['FY', 'FX']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'IA'], ['FY', 'FX']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA'], ['FY']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA'], ['FY', 'MZ']))).toBe(true);
  });

  it('detects missing sweep, step and output references', () => {
    const spec = makeSpec({ sweep: { variable: 'SA', start: 0, end: 1, points: 5 }, steps: [step('FZ', [1], 'hue')] });
    expect(referencesMissing(spec, base)).toBe(false);
    expect(referencesMissing(spec, model(['FZ'], ['FY']))).toBe(true);
    expect(referencesMissing(spec, model(['SA'], ['FY']))).toBe(true);
    expect(referencesMissing(spec, model(['FZ', 'SA'], ['FX']))).toBe(true);
  });
});
