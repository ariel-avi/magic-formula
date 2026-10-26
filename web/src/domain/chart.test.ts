import { describe, expect, it } from 'vitest';
import { MAX_SWEEPS, type ChartSpec, type SweepSpec } from './chart-spec';
import {
  addSweep,
  channelsChanged,
  needsCharacteristics,
  duplicateSweep,
  freezeBlankName,
  referencesMissing,
  removeSweep,
  renameSweep,
  replaceSweep,
  sweepDisplayName,
  sweepIndexOf,
  validateAxes,
} from './chart';
import { defaultSweepSpec } from './defaults';
import type { ModelInfo } from './model';

const model = (inNames: string[], outNames: string[], characteristicNames: string[] = ['DFY_DSA', 'DY']): ModelInfo => ({
  name: 'm',
  fittyp: null,
  useMode: null,
  tyreSide: 'LEFT',
  inputs: inNames.map((name) => ({ name, unit: '-', minimum: 0, minimumInclusive: true, negatedBy: name === 'FZ' ? 'VERTICAL' : name === 'SA' ? 'LATERAL' : 'NONE' })),
  outputs: outNames.map((name) => ({ name, unit: 'N', negatedBy: ['FY', 'MZ', 'MY'].includes(name) ? 'VERTICAL' : 'NONE' })),
  characteristics: characteristicNames.map((name) => ({ name, unit: 'N' })),
  limits: [],
});
const base = model(['FZ', 'SA', 'SR', 'IA'], ['FY', 'FX']);

const sweepOf = (id: string, name: string, variable = 'SA'): SweepSpec => ({
  id,
  name,
  swept: { variable, start: -1, end: 1, points: 5 },
  steps: [{ variable: 'FZ', values: [1, 2], colourMode: 'hue' }],
  fixed: { SR: 0, IA: 0 },
  uncombined: false,
});

const chartOf = (sweeps: SweepSpec[]): ChartSpec => ({
  id: 'c',
  sweeps,
  x: { kind: 'input', name: 'SA' },
  y: { kind: 'output', name: 'FY' },
});

describe('sweepDisplayName', () => {
  it('falls back to Sweep n for a blank name', () => {
    expect(sweepDisplayName(sweepOf('a', ''), 2)).toBe('Sweep 3');
    expect(sweepDisplayName(sweepOf('a', '   '), 2)).toBe('Sweep 3');
  });

  it('trims a name', () => {
    expect(sweepDisplayName(sweepOf('a', ' Front '), 0)).toBe('Front');
  });
});

describe('sweepIndexOf', () => {
  const sweeps = [sweepOf('a', 'A'), sweepOf('b', 'B')];
  it('finds the index or falls back to 0', () => {
    expect(sweepIndexOf(sweeps, 'b')).toBe(1);
    expect(sweepIndexOf(sweeps, 'zz')).toBe(0);
  });
});

describe('addSweep with a negative nominal load', () => {
  it('yields signed FZ defaults', () => {
    const entries = [{ section: 'S', key: 'FNOMIN', value: '-4000', quoted: false, line: 1, used: true }];
    const added = addSweep(chartOf([sweepOf('sweep-1', 'Sweep 1')]), base, entries).sweeps[1];
    expect(added.fixed.FZ).toBe(-4000);
    expect(added.steps[0].values).toEqual([-2000, -4000, -6000]);
    expect(added.swept).toEqual({ variable: 'SA', start: -15, end: 15, points: 61 });
  });
});

describe('addSweep', () => {
  it('appends the default sweep with a fresh id and name', () => {
    const chart = chartOf([sweepOf('sweep-1', 'Sweep 1')]);
    const next = addSweep(chart, base, []);
    expect(next.sweeps).toHaveLength(2);
    expect(next.sweeps[1]).toEqual(defaultSweepSpec(base, [], 'sweep-2', 'Sweep 2'));
    expect(chart.sweeps).toHaveLength(1);
  });

  it('reuses the lowest free id and name', () => {
    const chart = chartOf([sweepOf('sweep-2', 'Sweep 2'), sweepOf('sweep-3', 'Sweep 3')]);
    const added = addSweep(chart, base, []).sweeps[2];
    expect(added.id).toBe('sweep-1');
    expect(added.name).toBe('Sweep 1');
  });

  it('avoids the displayed name of a blank-named sweep', () => {
    const chart = chartOf([sweepOf('sweep-1', '')]);
    expect(addSweep(chart, base, []).sweeps[1].name).toBe('Sweep 2');
  });

  it('is ignored at the maximum', () => {
    const sweeps = Array.from({ length: MAX_SWEEPS }, (_, i) => sweepOf(`sweep-${i + 1}`, `Sweep ${i + 1}`));
    const chart = chartOf(sweeps);
    expect(addSweep(chart, base, [])).toBe(chart);
  });
});

describe('duplicateSweep', () => {
  it('inserts a deep copy after the source with a fresh identity', () => {
    const chart = chartOf([sweepOf('sweep-1', 'Sweep 1'), sweepOf('sweep-2', 'Sweep 2', 'SR')]);
    const next = duplicateSweep(chart, 0);
    expect(next.sweeps.map((s) => s.id)).toEqual(['sweep-1', 'sweep-3', 'sweep-2']);
    const copy = next.sweeps[1];
    expect(copy.name).toBe('Sweep 3');
    expect({ ...copy, id: '', name: '' }).toEqual({ ...chart.sweeps[0], id: '', name: '' });
    expect(next.sweeps[2]).toBe(chart.sweeps[1]);
    copy.steps[0].values.push(99);
    copy.fixed.SR = 5;
    expect(chart.sweeps[0].steps[0].values).toEqual([1, 2]);
    expect(chart.sweeps[0].fixed.SR).toBe(0);
  });

  it('is ignored at the maximum', () => {
    const sweeps = Array.from({ length: MAX_SWEEPS }, (_, i) => sweepOf(`sweep-${i + 1}`, `Sweep ${i + 1}`));
    const chart = chartOf(sweeps);
    expect(duplicateSweep(chart, 0)).toBe(chart);
  });
});

describe('removeSweep', () => {
  it('removes the sweep and keeps the axes', () => {
    const chart = chartOf([sweepOf('a', 'A'), sweepOf('b', 'B')]);
    const next = removeSweep(chart, 0);
    expect(next.sweeps).toEqual([chart.sweeps[1]]);
    expect(next.x).toBe(chart.x);
    expect(next.y).toBe(chart.y);
  });

  it('keeps the last sweep', () => {
    const chart = chartOf([sweepOf('a', 'A')]);
    expect(removeSweep(chart, 0)).toBe(chart);
  });
});

describe('renameSweep', () => {
  const chart = chartOf([sweepOf('a', 'A'), sweepOf('b', 'B')]);

  it('sets the name', () => {
    const next = renameSweep(chart, 1, 'Rear');
    expect(next.sweeps[1].name).toBe('Rear');
    expect(next.sweeps[0]).toBe(chart.sweeps[0]);
  });

  it('cuts a long name to 24 characters', () => {
    expect(renameSweep(chart, 0, 'x'.repeat(30)).sweeps[0].name).toBe('x'.repeat(24));
  });

  it('does not trim', () => {
    expect(renameSweep(chart, 0, '  a ').sweeps[0].name).toBe('  a ');
  });
});

describe('replaceSweep', () => {
  it('returns the same chart for the same sweep object', () => {
    const chart = chartOf([sweepOf('a', 'A')]);
    expect(replaceSweep(chart, 0, chart.sweeps[0])).toBe(chart);
  });

  it('makes X follow when the only sweep moves its swept input', () => {
    const chart = chartOf([sweepOf('a', 'A')]);
    const next = replaceSweep(chart, 0, sweepOf('a', 'A', 'SR'));
    expect(next.x).toEqual({ kind: 'input', name: 'SR' });
  });

  it('keeps X when another sweep still sweeps the old input', () => {
    const chart = chartOf([sweepOf('a', 'A'), sweepOf('b', 'B')]);
    expect(replaceSweep(chart, 0, sweepOf('a', 'A', 'SR')).x).toEqual({ kind: 'input', name: 'SA' });
  });

  it('keeps X when X is an output', () => {
    const chart: ChartSpec = { ...chartOf([sweepOf('a', 'A')]), x: { kind: 'output', name: 'SA' } };
    expect(replaceSweep(chart, 0, sweepOf('a', 'A', 'SR')).x).toEqual({ kind: 'output', name: 'SA' });
  });

  it('keeps X when X is an unrelated input', () => {
    const chart: ChartSpec = { ...chartOf([sweepOf('a', 'A')]), x: { kind: 'input', name: 'IA' } };
    expect(replaceSweep(chart, 0, sweepOf('a', 'A', 'SR')).x).toEqual({ kind: 'input', name: 'IA' });
  });

  it('keeps X when the swept variable is unchanged', () => {
    const chart = chartOf([sweepOf('a', 'A')]);
    const next = replaceSweep(chart, 0, sweepOf('a', 'Renamed'));
    expect(next).not.toBe(chart);
    expect(next.x).toBe(chart.x);
  });

  it('keeps the identity of the other sweeps', () => {
    const chart = chartOf([sweepOf('a', 'A'), sweepOf('b', 'B')]);
    const next = replaceSweep(chart, 0, sweepOf('a', 'A', 'SR'));
    expect(next.sweeps[1]).toBe(chart.sweeps[1]);
  });
});

describe('validateAxes', () => {
  it('accepts valid axes', () => {
    expect(validateAxes(chartOf([sweepOf('a', 'A')]), base)).toEqual([]);
  });

  it('reports an unknown X output', () => {
    const chart: ChartSpec = { ...chartOf([]), x: { kind: 'output', name: 'QQ' } };
    expect(validateAxes(chart, base)).toEqual(["Unknown X axis output 'QQ'."]);
  });

  it('reports an unknown Y input', () => {
    const chart: ChartSpec = { ...chartOf([]), y: { kind: 'input', name: 'QQ' } };
    expect(validateAxes(chart, base)).toEqual(["Unknown Y axis input 'QQ'."]);
  });

  it('reports both axes, X first', () => {
    const chart: ChartSpec = { ...chartOf([]), x: { kind: 'output', name: 'QQ' }, y: { kind: 'input', name: 'RR' } };
    expect(validateAxes(chart, base)).toEqual(["Unknown X axis output 'QQ'.", "Unknown Y axis input 'RR'."]);
  });
});

describe('needsCharacteristics', () => {
  const chart = chartOf([sweepOf('a', 'A')]);

  it('is true when X or Y shows a characteristic', () => {
    expect(needsCharacteristics({ ...chart, x: { kind: 'characteristic', name: 'DY' } })).toBe(true);
    expect(needsCharacteristics({ ...chart, y: { kind: 'characteristic', name: 'DY' } })).toBe(true);
  });

  it('is false for inputs and outputs', () => {
    expect(needsCharacteristics(chart)).toBe(false);
  });
});

describe('validateAxes characteristics', () => {
  it('accepts a known characteristic and reports an unknown one', () => {
    const known: ChartSpec = { ...chartOf([]), y: { kind: 'characteristic', name: 'DY' } };
    expect(validateAxes(known, base)).toEqual([]);
    const unknown: ChartSpec = { ...chartOf([]), y: { kind: 'characteristic', name: 'QQ' } };
    expect(validateAxes(unknown, base)).toEqual(["Unknown Y axis characteristic 'QQ'."]);
  });
});

describe('channelsChanged', () => {
  it('treats a missing previous model as changed', () => {
    expect(channelsChanged(null, base)).toBe(true);
  });

  it('is false for identical channels', () => {
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'IA'], ['FY', 'FX']))).toBe(false);
  });

  it('detects input and output differences', () => {
    expect(channelsChanged(base, model(['FZ'], ['FY', 'FX']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'VX'], ['FY', 'FX']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'IA'], ['FY']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'IA'], ['FY', 'MZ']))).toBe(true);
  });

  it('detects characteristic differences alone', () => {
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'IA'], ['FY', 'FX'], ['DFY_DSA']))).toBe(true);
    expect(channelsChanged(base, model(['FZ', 'SA', 'SR', 'IA'], ['FY', 'FX'], ['DFY_DSA', 'DX']))).toBe(true);
  });
});

describe('freezeBlankName', () => {
  it('keeps a named sweep untouched', () => {
    const chart = chartOf([sweepOf('a', 'A')]);
    expect(freezeBlankName(chart, 0)).toBe(chart);
  });

  it('stores the first free name for a blank sweep', () => {
    const chart = chartOf([sweepOf('a', 'Sweep 1'), sweepOf('b', '  ')]);
    expect(freezeBlankName(chart, 1).sweeps[1].name).toBe('Sweep 2');
  });

  it('keeps a frozen name unique after duplicating before it', () => {
    const chart = freezeBlankName(chartOf([sweepOf('a', 'Sweep 1'), sweepOf('b', 'Sweep 2'), sweepOf('c', '')]), 2);
    const copied = duplicateSweep(chart, 0);
    expect(copied.sweeps.map((sweep, i) => sweepDisplayName(sweep, i))).toEqual(['Sweep 1', 'Sweep 4', 'Sweep 2', 'Sweep 3']);
  });

  it('keeps a frozen name unique after removing an earlier sweep', () => {
    const chart = freezeBlankName(chartOf([sweepOf('a', 'Sweep 1'), sweepOf('b', 'Sweep 2'), sweepOf('c', '')]), 2);
    expect(removeSweep(chart, 0).sweeps.map((sweep, i) => sweepDisplayName(sweep, i))).toEqual(['Sweep 2', 'Sweep 3']);
  });
});

describe('referencesMissing', () => {
  const chart = chartOf([sweepOf('a', 'A'), sweepOf('b', 'B', 'SR')]);

  it('is false for a valid two-sweep chart', () => {
    expect(referencesMissing(chart, base)).toBe(false);
  });

  it('detects a missing swept variable in the second sweep', () => {
    expect(referencesMissing(chart, model(['FZ', 'SA', 'IA'], ['FY', 'FX']))).toBe(true);
  });

  it('detects a missing step variable', () => {
    expect(referencesMissing(chart, model(['SA', 'SR', 'IA'], ['FY', 'FX']))).toBe(true);
  });

  it('detects a missing X input', () => {
    expect(referencesMissing({ ...chart, x: { kind: 'input', name: 'VX' } }, base)).toBe(true);
  });

  it('detects a missing characteristic axis', () => {
    expect(referencesMissing({ ...chart, y: { kind: 'characteristic', name: 'DX' } }, base)).toBe(true);
    expect(referencesMissing({ ...chart, y: { kind: 'characteristic', name: 'DY' } }, base)).toBe(false);
  });

  it('detects a missing Y output', () => {
    expect(referencesMissing({ ...chart, y: { kind: 'output', name: 'MZ' } }, base)).toBe(true);
  });
});
