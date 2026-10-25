import { describe, expect, it } from 'vitest';
import { MAX_STEPS, type SweepSpec } from './chart-spec';
import { defaultSweptRange } from './defaults';
import type { InputChannel, TirEntry } from './model';
import { assignRole, INPUT_ROLES, roleBlocker, roleOf } from './roles';

const channel = (name: string, unit: string): InputChannel => ({
  name,
  unit,
  minimum: 0,
  minimumInclusive: true,
});

const FZ = channel('FZ', 'N');
const SA = channel('SA', 'rad');
const SR = channel('SR', '-');
const IA = channel('IA', 'rad');
const VX = channel('VX', 'm/s');

const entries: TirEntry[] = [
  { section: 'VERTICAL', key: 'FNOMIN', value: '5000', quoted: false, line: 1, used: true },
];

const baseSpec = (): SweepSpec => ({
  id: 'sweep-1',
  name: 'Sweep 1',
  swept: { variable: 'SA', start: -15, end: 15, points: 61 },
  steps: [{ variable: 'FZ', values: [2000, 4000, 6000], colourMode: 'hue' }],
  fixed: { FZ: 4000, SA: 0, SR: 0, IA: 2, VX: 16.7 },
  uncombined: false,
});

const twoSteps = (): SweepSpec => {
  const spec = baseSpec();
  spec.steps.push({ variable: 'IA', values: [0, 2], colourMode: 'saturation' });
  return spec;
};

const LOCK = 'One input is always swept: choose Sweep on another input to change this one.';

describe('INPUT_ROLES', () => {
  it('lists the three roles', () => {
    expect(INPUT_ROLES).toEqual(['sweep', 'step', 'value']);
  });
});

describe('roleOf', () => {
  it('tells sweep, step and value apart', () => {
    const spec = baseSpec();
    expect(roleOf(spec, 'SA')).toBe('sweep');
    expect(roleOf(spec, 'FZ')).toBe('step');
    expect(roleOf(spec, 'SR')).toBe('value');
  });
});

describe('roleBlocker', () => {
  it('locks the swept input', () => {
    const spec = baseSpec();
    expect(roleBlocker(spec, 'SA', 'step')).toBe(LOCK);
    expect(roleBlocker(spec, 'SA', 'value')).toBe(LOCK);
    expect(roleBlocker(spec, 'SA', 'sweep')).toBeNull();
  });

  it('lets a stepped input take any role', () => {
    for (const spec of [baseSpec(), twoSteps()]) {
      for (const role of INPUT_ROLES) {
        expect(roleBlocker(spec, 'FZ', role)).toBeNull();
      }
    }
  });

  it('lets a held input become a sweep or stay a value', () => {
    const spec = baseSpec();
    expect(roleBlocker(spec, 'SR', 'sweep')).toBeNull();
    expect(roleBlocker(spec, 'SR', 'value')).toBeNull();
  });

  it('allows a step while fewer than the maximum are used', () => {
    expect(roleBlocker(baseSpec(), 'SR', 'step')).toBeNull();
  });

  it('blocks a step at the maximum', () => {
    expect(roleBlocker(twoSteps(), 'VX', 'step')).toBe(`At most ${MAX_STEPS} inputs can be stepped.`);
  });
});

describe('assignRole', () => {
  it('returns the same spec when the role is unchanged', () => {
    const spec = baseSpec();
    expect(assignRole(spec, SR, 'value', entries)).toBe(spec);
  });

  it('returns the same spec when the role is blocked', () => {
    const spec = baseSpec();
    expect(assignRole(spec, SA, 'value', entries)).toBe(spec);
    const full = twoSteps();
    expect(assignRole(full, VX, 'step', entries)).toBe(full);
  });

  it('moves the sweep and turns the old sweep into a held value', () => {
    const spec = baseSpec();
    const next = assignRole(spec, SR, 'sweep', entries);
    expect(next.swept).toEqual(defaultSweptRange(SR, entries));
    expect(roleOf(next, 'SA')).toBe('value');
    expect(next.fixed.SA).toBe(0);
    expect(next.steps).toEqual(spec.steps);
  });

  it('removes the step of an input that becomes the sweep', () => {
    const next = assignRole(baseSpec(), FZ, 'sweep', entries);
    expect(next.steps).toEqual([]);
    expect(next.swept.variable).toBe('FZ');
  });

  it('steps a held input from its held value with the complementary colour mode', () => {
    const next = assignRole(baseSpec(), IA, 'step', entries);
    expect(next.steps[1]).toEqual({ variable: 'IA', values: [2], colourMode: 'saturation' });
  });

  it('falls back to the default value when the held value is not finite', () => {
    const spec = { ...baseSpec(), steps: [], fixed: { ...baseSpec().fixed, FZ: NaN } };
    expect(assignRole(spec, FZ, 'step', entries).steps).toEqual([
      { variable: 'FZ', values: [5000], colourMode: 'hue' },
    ]);
  });

  it('falls back to the default value when no held value exists', () => {
    const spec = { ...baseSpec(), steps: [], fixed: {} };
    expect(assignRole(spec, FZ, 'step', entries).steps[0].values).toEqual([5000]);
  });

  it('turns a stepped input into a held value and keeps its value', () => {
    const next = assignRole(baseSpec(), FZ, 'value', entries);
    expect(next.steps).toEqual([]);
    expect(next.fixed.FZ).toBe(4000);
  });

  it('keeps id, name, fixed and uncombined when assigning a role', () => {
    const spec: SweepSpec = { ...baseSpec(), name: 'Custom', uncombined: true };
    for (const [target, role] of [
      [SR, 'sweep'],
      [IA, 'step'],
      [FZ, 'value'],
    ] as const) {
      const next = assignRole(spec, target, role, entries);
      expect(next.id).toBe(spec.id);
      expect(next.name).toBe('Custom');
      expect(next.fixed).toEqual(spec.fixed);
      expect(next.uncombined).toBe(true);
    }
  });
});
