import type { CoordinateSystem } from './model';

/** World = ISO axes: x forward, y left, z up; tire radius 1. */
export type Vec3 = readonly [number, number, number];

/** Proper rotation into three.js y-up coordinates. */
export function toThree(v: Vec3): Vec3 {
  return [v[0], v[2], 0 - v[1]];
}

export const BASES: Record<CoordinateSystem, { x: Vec3; y: Vec3; z: Vec3 }> = {
  ISO: { x: [1, 0, 0], y: [0, 1, 0], z: [0, 0, 1] },
  SAE: { x: [1, 0, 0], y: [0, -1, 0], z: [0, 0, -1] },
};

export type SceneRole = 'axis' | 'force' | 'moment' | 'angle' | 'velocity' | 'spin';

export interface SceneArrow {
  id: 'x' | 'y' | 'z' | 'fx' | 'fy' | 'fz' | 'heading' | 'travel';
  role: SceneRole;
  origin: Vec3;
  direction: Vec3;
  length: number;
  label: string;
}

export interface SceneArc {
  id: 'mx' | 'my' | 'mz' | 'alpha' | 'gamma' | 'spin';
  role: SceneRole;
  center: Vec3;
  axis: Vec3;
  from: Vec3;
  angle: number;
  radius: number;
  label: string;
}

export interface TireScene {
  system: CoordinateSystem;
  title: string;
  tire: { center: Vec3; radius: number; width: number; camber: number };
  arrows: SceneArrow[];
  arcs: SceneArc[];
  captions: string[];
}

export function add(a: Vec3, b: Vec3): Vec3 {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

export function scale(v: Vec3, s: number): Vec3 {
  return [v[0] * s, v[1] * s, v[2] * s];
}

export function dot(a: Vec3, b: Vec3): number {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

export function cross(a: Vec3, b: Vec3): Vec3 {
  return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
}

export function normalize(v: Vec3): Vec3 {
  return scale(v, 1 / Math.hypot(v[0], v[1], v[2]));
}

/** Rodrigues rotation of `v` about normalize(axis) by `angle` (right-hand rule). */
export function rotate(v: Vec3, axis: Vec3, angle: number): Vec3 {
  const k = normalize(axis);
  const c = Math.cos(angle);
  return add(add(scale(v, c), scale(cross(k, v), Math.sin(angle))), scale(k, dot(k, v) * (1 - c)));
}

/** segments + 1 points: center + radius * rotate(from, axis, angle * i / segments). */
export function arcPoints(arc: SceneArc, segments: number): Vec3[] {
  return Array.from({ length: segments + 1 }, (_, i) =>
    add(arc.center, scale(rotate(arc.from, arc.axis, (arc.angle * i) / segments), arc.radius)),
  );
}

/** The last arc point and the unit tangent there (arrow-head placement). */
export function arcEnd(arc: SceneArc): { point: Vec3; tangent: Vec3 } {
  const end = rotate(arc.from, arc.axis, arc.angle);
  return {
    point: add(arc.center, scale(end, arc.radius)),
    tangent: normalize(cross(normalize(arc.axis), end)),
  };
}

export const ALPHA_DEMO_RAD = (12 * Math.PI) / 180;
export const GAMMA_DEMO_RAD = (6 * Math.PI) / 180;

const ORIGIN: Vec3 = [0, 0, 0];
const WORLD_X: Vec3 = [1, 0, 0];
const WORLD_Y: Vec3 = [0, 1, 0];
const WORLD_Z: Vec3 = [0, 0, 1];

const TITLES: Record<CoordinateSystem, string> = {
  ISO: 'ISO tire axis system',
  SAE: 'SAE tire axis system',
};

const AXIS_LABELS: Record<CoordinateSystem, { x: string; y: string; z: string }> = {
  ISO: { x: 'x (forward)', y: 'y (left)', z: 'z (up)' },
  SAE: { x: 'x (forward)', y: 'y (right)', z: 'z (down)' },
};

const CAPTIONS: Record<CoordinateSystem, string[]> = {
  ISO: [
    'ISO: x forward, y left, z up',
    'Loaded tire: Fz > 0',
    'α > 0: travel to the right of the heading; dFy/dα > 0',
    'κ > 0: driving, Fx > 0',
    'γ > 0: top tilted to the right',
  ],
  SAE: [
    'SAE: x forward, y right, z down',
    'Loaded tire: Fz < 0',
    'α > 0: travel to the right of the heading; dFy/dα < 0',
    'κ > 0: driving, Fx > 0',
    'γ > 0: top tilted to the right',
  ],
};

/** The convention diagram of a system as pure geometry: the system enters only through BASES and the per-system tables. */
export function tireScene(system: CoordinateSystem): TireScene {
  const basis = BASES[system];
  const labels = AXIS_LABELS[system];
  const center = rotate(WORLD_Z, WORLD_X, GAMMA_DEMO_RAD);
  const moment = (id: 'mx' | 'my' | 'mz', axis: Vec3, from: Vec3, label: string): SceneArc => ({
    id,
    role: 'moment',
    center: scale(axis, 1.25),
    axis,
    from,
    angle: 1.5 * Math.PI,
    radius: 0.25,
    label,
  });
  const axisArrow = (id: 'x' | 'y' | 'z', direction: Vec3, label: string): SceneArrow => ({
    id,
    role: 'axis',
    origin: ORIGIN,
    direction,
    length: 1.6,
    label,
  });
  const forceArrow = (id: 'fx' | 'fy' | 'fz', direction: Vec3, label: string): SceneArrow => ({
    id,
    role: 'force',
    origin: ORIGIN,
    direction,
    length: 1.1,
    label,
  });
  return {
    system,
    title: TITLES[system],
    tire: { center, radius: 1, width: 0.35, camber: GAMMA_DEMO_RAD },
    arrows: [
      axisArrow('x', basis.x, labels.x),
      axisArrow('y', basis.y, labels.y),
      axisArrow('z', basis.z, labels.z),
      forceArrow('fx', basis.x, '+Fx'),
      forceArrow('fy', basis.y, '+Fy'),
      forceArrow('fz', basis.z, '+Fz'),
      { id: 'heading', role: 'velocity', origin: ORIGIN, direction: WORLD_X, length: 2.2, label: 'heading' },
      {
        id: 'travel',
        role: 'velocity',
        origin: ORIGIN,
        direction: rotate(WORLD_X, WORLD_Z, -ALPHA_DEMO_RAD),
        length: 2.2,
        label: 'V',
      },
    ],
    arcs: [
      moment('mx', basis.x, basis.y, '+Mx'),
      moment('my', basis.y, basis.z, '+My'),
      moment('mz', basis.z, basis.x, '+Mz'),
      {
        id: 'alpha',
        role: 'angle',
        center: ORIGIN,
        axis: [0, 0, -1],
        from: WORLD_X,
        angle: ALPHA_DEMO_RAD,
        radius: 1.9,
        label: 'α > 0',
      },
      {
        id: 'gamma',
        role: 'angle',
        center: ORIGIN,
        axis: WORLD_X,
        from: WORLD_Z,
        angle: GAMMA_DEMO_RAD,
        radius: 2,
        label: 'γ > 0',
      },
      {
        id: 'spin',
        role: 'spin',
        center,
        axis: rotate(WORLD_Y, WORLD_X, GAMMA_DEMO_RAD),
        from: center,
        angle: Math.PI / 2,
        radius: 1.15,
        label: 'Ω (SR > 0: driving)',
      },
    ],
    captions: CAPTIONS[system],
  };
}
