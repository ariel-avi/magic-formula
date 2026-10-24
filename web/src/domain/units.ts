/** Presentation units. The solver works in SI; charts and editors show these units. */
export interface DisplayUnit {
  label: string;
  toSi(value: number): number;
  fromSi(value: number): number;
}

function identity(label: string): DisplayUnit {
  return { label, toSi: (value) => value, fromSi: (value) => value };
}

const UNITS: Record<string, DisplayUnit> = {
  rad: {
    label: 'deg',
    toSi: (value) => (value * Math.PI) / 180,
    fromSi: (value) => (value * 180) / Math.PI,
  },
  'N*m': identity('N·m'),
};

export function displayUnit(siUnit: string): DisplayUnit {
  return UNITS[siUnit] ?? identity(siUnit);
}
