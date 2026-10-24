import { describe, expect, it } from 'vitest';
import { buildBatch } from './batch';
import type { ChartSpec } from './chart-spec';
import type { InputChannel } from './model';

const channel = (name: string, unit: string): InputChannel => ({ name, unit, minimum: 0, minimumInclusive: true });
const layout = [channel('FZ', 'N'), channel('SA', 'rad'), channel('SR', '-'), channel('IA', 'rad'), channel('VX', 'm/s')];

const spec: ChartSpec = {
  id: 'c',
  sweep: { variable: 'SA', start: -10, end: 10, points: 3 },
  steps: [{ variable: 'FZ', values: [3000, 5000], colourMode: 'hue' }],
  fixed: { SR: 0.1, IA: 2, VX: 16.7, P: 200000 },
  output: 'FY',
  uncombined: false,
};

describe('buildBatch', () => {
  const batch = buildBatch(spec, layout);
  const at = (k: number, r: number) => batch.inputs[k * batch.rows + r];

  it('has one row per combination and sweep point', () => {
    expect(batch.rows).toBe(6);
    expect(batch.inputs).toHaveLength(6 * 5);
    expect(batch.grid).toEqual([-10, 0, 10]);
    expect(batch.combinations).toHaveLength(2);
  });

  it('converts the swept column to radians, column-major', () => {
    [-10, 0, 10, -10, 0, 10].forEach((deg, r) => expect(at(1, r)).toBeCloseTo((deg * Math.PI) / 180, 12));
  });

  it('keeps a step column constant within a combination', () => {
    [3000, 3000, 3000, 5000, 5000, 5000].forEach((fz, r) => expect(at(0, r)).toBe(fz));
  });

  it('fills fixed columns and converts degrees to radians', () => {
    for (let r = 0; r < 6; r++) {
      expect(at(2, r)).toBe(0.1);
      expect(at(3, r)).toBeCloseTo((2 * Math.PI) / 180, 12);
      expect(at(4, r)).toBe(16.7);
    }
  });

  it('follows the layout order including extra channels', () => {
    const withPressure = buildBatch(spec, [...layout, channel('P', 'Pa')]);
    expect(withPressure.inputs).toHaveLength(6 * 6);
    expect(withPressure.inputs[5 * 6 + 3]).toBe(200000);
    expect(withPressure.inputs[0]).toBe(3000);
  });

  it('works without steps', () => {
    const plain = buildBatch({ ...spec, steps: [], fixed: { ...spec.fixed, FZ: 4000 } }, layout);
    expect(plain.rows).toBe(3);
    expect(plain.inputs[0]).toBe(4000);
  });
});
