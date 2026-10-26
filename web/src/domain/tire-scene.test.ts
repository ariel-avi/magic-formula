import { describe, expect, it } from 'vitest';
import type { CoordinateSystem } from './model';
import {
  ALPHA_DEMO_RAD,
  BASES,
  GAMMA_DEMO_RAD,
  add,
  arcEnd,
  arcPoints,
  cross,
  dot,
  normalize,
  rotate,
  scale,
  tireScene,
  toThree,
  type SceneArc,
  type Vec3,
} from './tire-scene';

const close = (a: Vec3, b: Vec3, digits = 12) => {
  for (let i = 0; i < 3; i += 1) {
    expect(a[i]).toBeCloseTo(b[i], digits);
  }
};
const sub = (a: Vec3, b: Vec3): Vec3 => add(a, scale(b, -1));
const SYSTEMS: CoordinateSystem[] = ['ISO', 'SAE'];
const arc = (scene: ReturnType<typeof tireScene>, id: SceneArc['id']): SceneArc => scene.arcs.find((a) => a.id === id)!;
const arrow = (scene: ReturnType<typeof tireScene>, id: string) => scene.arrows.find((a) => a.id === id)!;

describe('vector helpers', () => {
  it('computes add, scale, dot, cross and normalize', () => {
    expect(add([1, 2, 3], [4, 5, 6])).toEqual([5, 7, 9]);
    expect(scale([1, 2, 3], 2)).toEqual([2, 4, 6]);
    expect(dot([1, 2, 3], [4, 5, 6])).toBe(32);
    expect(cross([1, 0, 0], [0, 1, 0])).toEqual([0, 0, 1]);
    close(normalize([0, 3, 4]), [0, 0.6, 0.8]);
  });
});

describe('toThree', () => {
  const units: Vec3[] = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1],
  ];
  it('preserves lengths and dot products', () => {
    const a: Vec3 = [1, -2, 3];
    const b: Vec3 = [0.5, 4, -1];
    expect(dot(toThree(a), toThree(a))).toBeCloseTo(dot(a, a), 12);
    expect(dot(toThree(a), toThree(b))).toBeCloseTo(dot(a, b), 12);
  });
  it('has determinant +1', () => {
    const [i, j, k] = units.map(toThree);
    expect(dot(i, cross(j, k))).toBeCloseTo(1, 12);
  });
  it('maps up to y and left to -z', () => {
    expect(toThree([0, 0, 1])).toEqual([0, 1, 0]);
    expect(toThree([0, 1, 0])).toEqual([0, 0, -1]);
  });
});

describe('rotate', () => {
  it('turns x about z by 90 degrees onto y', () => {
    close(rotate([1, 0, 0], [0, 0, 1], Math.PI / 2), [0, 1, 0]);
  });
  it('normalises the axis', () => {
    close(rotate([1, 0, 0], [0, 0, 5], Math.PI / 2), [0, 1, 0]);
  });
});

describe('arcs', () => {
  const sample: SceneArc = {
    id: 'mz',
    role: 'moment',
    center: [1, 2, 3],
    axis: [0, 0, 1],
    from: [1, 0, 0],
    angle: Math.PI / 2,
    radius: 2,
    label: 'x',
  };
  it('arcPoints has segments + 1 points from the start to the end', () => {
    const points = arcPoints(sample, 8);
    expect(points).toHaveLength(9);
    close(points[0], [3, 2, 3]);
    close(points[8], [1, 4, 3]);
  });
  it('arcEnd tangent is perpendicular to the radius and the axis', () => {
    const { point, tangent } = arcEnd(sample);
    close(point, [1, 4, 3]);
    expect(dot(tangent, sub(point, sample.center))).toBeCloseTo(0, 12);
    expect(dot(tangent, sample.axis)).toBeCloseTo(0, 12);
    close(tangent, [-1, 0, 0]);
  });
});

describe.each(SYSTEMS)('tireScene %s', (system) => {
  const scene = tireScene(system);
  it('uses the system basis for axes and forces', () => {
    for (const [axis, force] of [
      ['x', 'fx'],
      ['y', 'fy'],
      ['z', 'fz'],
    ] as const) {
      expect(arrow(scene, axis).direction).toEqual(BASES[system][axis]);
      expect(arrow(scene, force).direction).toEqual(BASES[system][axis]);
    }
  });
  it('travels to the right of the heading and the alpha arc ends on the travel direction', () => {
    expect(arrow(scene, 'travel').direction[1]).toBeLessThan(0);
    expect(arrow(scene, 'heading').direction).toEqual([1, 0, 0]);
    close(arcEnd(arc(scene, 'alpha')).point, scale(arrow(scene, 'travel').direction, arc(scene, 'alpha').radius));
    expect(arc(scene, 'alpha').angle).toBe(ALPHA_DEMO_RAD);
  });
  it('tilts the tire top to the right', () => {
    expect(scene.tire.center[1]).toBeLessThan(0);
    expect(scene.tire.camber).toBe(GAMMA_DEMO_RAD);
    expect(arc(scene, 'gamma').angle).toBe(GAMMA_DEMO_RAD);
  });
  it('has only positive arc angles', () => {
    for (const a of scene.arcs) {
      expect(a.angle).toBeGreaterThan(0);
    }
  });
});

describe('per-system differences', () => {
  it('flips y and z for SAE', () => {
    const iso = tireScene('ISO');
    const sae = tireScene('SAE');
    expect(arrow(iso, 'z').direction).toEqual([0, 0, 1]);
    expect(arrow(sae, 'z').direction).toEqual([0, 0, -1]);
    expect(arrow(iso, 'y').direction).toEqual([0, 1, 0]);
    expect(arrow(sae, 'y').direction).toEqual([0, -1, 0]);
    expect(arrow(iso, 'y').label).toBe('y (left)');
    expect(arrow(sae, 'y').label).toBe('y (right)');
    expect(arrow(iso, 'z').label).toBe('z (up)');
    expect(arrow(sae, 'z').label).toBe('z (down)');
    expect(arrow(sae, 'x').label).toBe('x (forward)');
  });

  it('reverses the Mz arc sense', () => {
    const sense = (system: CoordinateSystem) => {
      const mz = arc(tireScene(system), 'mz');
      const [p0, p1] = arcPoints(mz, 8);
      return dot(cross(sub(p0, mz.center), sub(p1, mz.center)), [0, 0, 1]);
    };
    expect(sense('ISO')).toBeGreaterThan(0);
    expect(sense('SAE')).toBeLessThan(0);
  });

  it('has the titles and captions', () => {
    expect(tireScene('ISO').title).toBe('ISO tire axis system');
    expect(tireScene('SAE').title).toBe('SAE tire axis system');
    expect(tireScene('ISO').captions).toEqual([
      'ISO: x forward, y left, z up',
      'Loaded tire: Fz > 0',
      'α > 0: travel to the right of the heading; dFy/dα > 0',
      'κ > 0: driving, Fx > 0',
      'γ > 0: top tilted to the right',
    ]);
    expect(tireScene('SAE').captions).toEqual([
      'SAE: x forward, y right, z down',
      'Loaded tire: Fz < 0',
      'α > 0: travel to the right of the heading; dFy/dα < 0',
      'κ > 0: driving, Fx > 0',
      'γ > 0: top tilted to the right',
    ]);
  });
});
