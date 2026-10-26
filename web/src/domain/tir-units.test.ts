import { describe, expect, it } from 'vitest';
import { SI_UNIT_NAMES, unitOptions } from './tir-units';

describe('SI_UNIT_NAMES', () => {
  it('names the SI unit of every dimensional key', () => {
    expect(SI_UNIT_NAMES).toEqual({
      LENGTH: 'meter',
      FORCE: 'newton',
      ANGLE: 'radians',
      MASS: 'kg',
      TIME: 'second',
      PRESSURE: 'pascal',
    });
  });
});

describe('unitOptions', () => {
  it('lists the SI unit first and supported, and the others unsupported', () => {
    const options = unitOptions('LENGTH', 'meter');
    expect(options[0]).toEqual({ value: 'meter', supported: true });
    expect(options.filter((o) => o.value === 'meter')).toHaveLength(1);
    expect(options.slice(1).every((o) => !o.supported)).toBe(true);
  });

  it('matches the key and value case-insensitively without duplicates', () => {
    const options = unitOptions('length', 'Meter');
    expect(options[0]).toEqual({ value: 'Meter', supported: true });
    expect(options.some((o) => o.value === 'meter')).toBe(false);
  });

  it('lists a known non-SI value once, unsupported', () => {
    const options = unitOptions('LENGTH', 'millimeter');
    expect(options.map((o) => o.value)).toEqual([
      'meter',
      'millimeter',
      'centimeter',
      'kilometer',
      'inch',
      'foot',
      'mile',
    ]);
    expect(options[1]).toEqual({ value: 'millimeter', supported: false });
  });

  it('prepends an unknown value as unsupported', () => {
    const options = unitOptions('LENGTH', 'furlong');
    expect(options[0]).toEqual({ value: 'furlong', supported: false });
    expect(options).toHaveLength(8);
  });

  it('does not list a blank value', () => {
    expect(unitOptions('ANGLE', '  ')).toEqual([
      { value: 'radians', supported: true },
      { value: 'degrees', supported: false },
    ]);
  });

  it('keeps the value of an unknown key', () => {
    expect(unitOptions('FOO', 'bar')).toEqual([{ value: 'bar', supported: true }]);
  });

  it('lists nothing for an unknown key without a value', () => {
    expect(unitOptions('FOO', '')).toEqual([]);
  });
});
