import {
  MAX_STEPS,
  MAX_STEP_VALUES,
  MAX_SWEEP_POINTS,
  MIN_SWEEP_POINTS,
  type ColourMode,
  type StepSpec,
  type SweepSpec,
} from './chart-spec';
import type { InputChannel } from './model';

/** One element per step. */
export interface Combination {
  indices: number[];
  values: number[];
}

/** Cartesian product of the step values, step 0 outermost. No steps give one empty combination. */
export function stepCombinations(steps: readonly StepSpec[]): Combination[] {
  let combinations: Combination[] = [{ indices: [], values: [] }];
  for (const step of steps) {
    combinations = combinations.flatMap((combination) =>
      step.values.map((value, index) => ({
        indices: [...combination.indices, index],
        values: [...combination.values, value],
      })),
    );
  }
  return combinations;
}

/** Inputs that `stepIndex` may use: not the swept variable and not the variable of another step. */
export function availableStepVariables(
  spec: SweepSpec,
  inputs: readonly InputChannel[],
  stepIndex: number,
): string[] {
  const taken = new Set([
    spec.swept.variable,
    ...spec.steps.filter((_, index) => index !== stepIndex).map((step) => step.variable),
  ]);
  return inputs.map((channel) => channel.name).filter((name) => !taken.has(name));
}

const COMPLEMENT: Record<ColourMode, ColourMode> = { hue: 'saturation', saturation: 'hue' };

/** Appends a step; ignored at the maximum. A second step gets the colour mode the first one does not use. */
export function addStep(steps: readonly StepSpec[], variable: string, values: number[]): StepSpec[] {
  if (steps.length >= MAX_STEPS) {
    return [...steps];
  }
  const colourMode: ColourMode = steps.length === 0 ? 'hue' : COMPLEMENT[steps[0].colourMode];
  return [...steps, { variable, values, colourMode }];
}

export function removeStep(steps: readonly StepSpec[], index: number): StepSpec[] {
  return steps.filter((_, i) => i !== index);
}

/** With two steps the other step takes the complementary mode, so the two modes always differ. */
export function setColourMode(steps: readonly StepSpec[], index: number, mode: ColourMode): StepSpec[] {
  return steps.map((step, i) => {
    if (i === index) {
      return { ...step, colourMode: mode };
    }
    return steps.length === 2 ? { ...step, colourMode: COMPLEMENT[mode] } : step;
  });
}

/** Returns one message per violated rule; an empty array means the spec is valid. */
export function validateSweepSpec(spec: SweepSpec, inputs: readonly InputChannel[]): string[] {
  const messages: string[] = [];
  const known = (name: string) => inputs.some((channel) => channel.name === name);
  const { swept, steps } = spec;

  if (!known(swept.variable)) {
    messages.push(`Unknown sweep variable '${swept.variable}'.`);
  }
  if (!Number.isInteger(swept.points) || swept.points < MIN_SWEEP_POINTS || swept.points > MAX_SWEEP_POINTS) {
    messages.push(`Sweep points must be an integer from ${MIN_SWEEP_POINTS} to ${MAX_SWEEP_POINTS}.`);
  }
  if (!Number.isFinite(swept.start) || !Number.isFinite(swept.end) || swept.start === swept.end) {
    messages.push('Sweep start and end must be finite and different.');
  }
  if (steps.length > MAX_STEPS) {
    messages.push(`At most ${MAX_STEPS} step variables are allowed.`);
  }
  steps.forEach((step, k) => {
    const name = step.variable;
    if (!known(name)) {
      messages.push(`Unknown step variable '${name}'.`);
    }
    if (name === swept.variable) {
      messages.push(`Step variable '${name}' is the sweep variable.`);
    }
    if (steps.slice(0, k).some((earlier) => earlier.variable === name)) {
      messages.push(`Step variable '${name}' is used twice.`);
    }
    if (step.values.length < 1 || step.values.length > MAX_STEP_VALUES) {
      messages.push(`Step '${name}' needs 1 to ${MAX_STEP_VALUES} values.`);
    }
    if (step.values.some((value) => !Number.isFinite(value))) {
      messages.push(`Step '${name}' has a value that is not a finite number.`);
    }
  });
  if (steps.length === 2 && steps[0].colourMode === steps[1].colourMode) {
    messages.push('The two steps must use different colour modes.');
  }
  for (const channel of inputs) {
    const driven = channel.name === swept.variable || steps.some((step) => step.variable === channel.name);
    if (!driven && !Number.isFinite(spec.fixed[channel.name])) {
      messages.push(`Fixed value of '${channel.name}' must be a finite number.`);
    }
  }
  return messages;
}
