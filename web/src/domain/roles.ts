import { MAX_STEPS, type ChartSpec } from './chart-spec';
import { defaultFixedValue, defaultSweep } from './defaults';
import type { InputChannel, TirEntry } from './model';
import { addStep } from './steps';

export type InputRole = 'sweep' | 'step' | 'value';

export const INPUT_ROLES: readonly InputRole[] = ['sweep', 'step', 'value'];

/** 'sweep' for the swept input, 'step' for a stepped input, otherwise 'value'. */
export function roleOf(spec: ChartSpec, name: string): InputRole {
  if (name === spec.sweep.variable) {
    return 'sweep';
  }
  return spec.steps.some((step) => step.variable === name) ? 'step' : 'value';
}

/** Why `name` cannot take `role` now; null when it is allowed or already that role. */
export function roleBlocker(spec: ChartSpec, name: string, role: InputRole): string | null {
  const current = roleOf(spec, name);
  if (role === current) {
    return null;
  }
  if (current === 'sweep') {
    return 'One input is always swept: choose Sweep on another input to change this one.';
  }
  if (role === 'step' && spec.steps.length >= MAX_STEPS) {
    return `At most ${MAX_STEPS} inputs can be stepped.`;
  }
  return null;
}

/** Gives `channel` the role. Returns the same spec object when the role is unchanged or blocked. */
export function assignRole(
  spec: ChartSpec,
  channel: InputChannel,
  role: InputRole,
  entries: readonly TirEntry[],
): ChartSpec {
  const name = channel.name;
  if (roleOf(spec, name) === role || roleBlocker(spec, name, role) !== null) {
    return spec;
  }
  const withoutStep = spec.steps.filter((step) => step.variable !== name);
  if (role === 'sweep') {
    return { ...spec, sweep: defaultSweep(channel, entries), steps: withoutStep };
  }
  if (role === 'step') {
    const held = spec.fixed[name];
    const start = Number.isFinite(held) ? held : defaultFixedValue(channel, entries);
    return { ...spec, steps: addStep(spec.steps, name, [start]) };
  }
  return { ...spec, steps: withoutStep };
}
