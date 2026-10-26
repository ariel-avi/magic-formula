import { describe, expect, it } from 'vitest';
import { displayUnit, hasUnit } from './units';

describe('hasUnit', () => {
  it('is false for empty and dash labels', () => {
    expect(hasUnit('')).toBe(false);
    expect(hasUnit('-')).toBe(false);
  });

  it('is true for a real unit', () => {
    expect(hasUnit('N')).toBe(true);
  });
});

describe('displayUnit', () => {
  it('shows radians as degrees', () => {
    const unit = displayUnit('rad');
    expect(unit.label).toBe('deg');
    expect(unit.toSi(180)).toBeCloseTo(Math.PI, 12);
    expect(unit.fromSi(Math.PI)).toBeCloseTo(180, 10);
  });

  it('round-trips degrees', () => {
    const unit = displayUnit('rad');
    expect(unit.fromSi(unit.toSi(37.5))).toBeCloseTo(37.5, 10);
  });

  it('shows N/rad as N/deg and converts both ways', () => {
    const unit = displayUnit('N/rad');
    expect(unit.label).toBe('N/deg');
    expect(unit.fromSi(180 / Math.PI)).toBeCloseTo(1, 12);
    const x = -55384.61537396;
    expect(unit.toSi(unit.fromSi(x))).toBeCloseTo(x, 8);
  });

  it('prints N*m with a middle dot and keeps the value', () => {
    const unit = displayUnit('N*m');
    expect(unit.label).toBe('N·m');
    expect(unit.toSi(5)).toBe(5);
    expect(unit.fromSi(5)).toBe(5);
  });

  it.each(['N', 'm/s', '-'])('keeps %s unchanged', (si) => {
    const unit = displayUnit(si);
    expect(unit.label).toBe(si);
    expect(unit.toSi(2.5)).toBe(2.5);
    expect(unit.fromSi(2.5)).toBe(2.5);
  });
});
