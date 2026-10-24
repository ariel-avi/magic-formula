export interface UnitOption {
  value: string;
  supported: boolean;
}

// Only the SI name is accepted by the solver. The others are listed for display and are never selectable.
const UNIT_TABLE: Readonly<Record<string, { si: string; others: readonly string[] }>> = {
  LENGTH: { si: 'meter', others: ['millimeter', 'centimeter', 'kilometer', 'inch', 'foot', 'mile'] },
  FORCE: { si: 'newton', others: ['kilonewton', 'pound_force', 'kilogram_force', 'dyne'] },
  ANGLE: { si: 'radians', others: ['degrees'] },
  MASS: { si: 'kg', others: ['gram', 'tonne', 'pound_mass', 'slug'] },
  TIME: { si: 'second', others: ['millisecond', 'minute', 'hour'] },
  PRESSURE: { si: 'pascal', others: ['kilopascal', 'bar', 'psi'] },
};

/** The unit name the solver accepts for each dimensional [UNITS] key. */
export const SI_UNIT_NAMES: Readonly<Record<string, string>> = Object.fromEntries(
  Object.entries(UNIT_TABLE).map(([key, row]) => [key, row.si]),
);

/** Options for a [UNITS] entry; the file's value `current` appears exactly once, and a blank value is not listed. */
export function unitOptions(key: string, current: string): UnitOption[] {
  const row = UNIT_TABLE[key.toUpperCase()];
  const blank = current.trim() === '';
  if (row === undefined) {
    return blank ? [] : [{ value: current, supported: true }];
  }
  const options = [row.si, ...row.others].map((value) => ({ value, supported: value === row.si }));
  if (blank) {
    return options;
  }
  const wanted = current.trim().toLowerCase();
  const index = options.findIndex((option) => option.value.toLowerCase() === wanted);
  if (index >= 0) {
    options[index] = { ...options[index], value: current };
    return options;
  }
  return [{ value: current, supported: false }, ...options];
}
