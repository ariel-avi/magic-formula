import { describeBatchRow, type SweepBatch } from './batch';
import type { ChartSpec, SweepSpec } from './chart-spec';
import type {
  BatchWarning,
  Classification,
  Conversion,
  CoordinateIdentification,
  CoordinateSystem,
  AxisDecision,
  IdentificationAxis,
  InputChannel,
  ReadingStatus,
  Transforms,
} from './model';

export const SYSTEM_LABELS: Record<CoordinateSystem, string> = { ISO: 'ISO', SAE: 'SAE' };

export const CLASSIFICATION_LABELS: Record<Classification, string> = { ISO: 'ISO', SAE: 'SAE', MIXED: 'Mixed' };

type Sign = 'POSITIVE' | 'NEGATIVE';

const SIDE: Record<Sign, string> = { POSITIVE: 'ISO', NEGATIVE: 'SAE' };
const RELATION: Record<Sign, string> = { POSITIVE: '>', NEGATIVE: '<' };
const MIXED_SCENE: Record<Sign, CoordinateSystem> = { POSITIVE: 'ISO', NEGATIVE: 'SAE' };

/** The identified system; null when nothing was identified or the model mixes conventions. */
export function activeSystem(id: CoordinateIdentification | null): CoordinateSystem | null {
  return id === null || id.classification === 'MIXED' ? null : id.classification;
}

/** The system the 3D view draws: the classification, the vertical side for a mixed model, ISO without identification. */
export function sceneSystem(id: CoordinateIdentification | null): CoordinateSystem {
  if (id === null) {
    return 'ISO';
  }
  return id.classification === 'MIXED' ? MIXED_SCENE[id.effective.vertical] : id.classification;
}

const assumed = (decision: AxisDecision): string => (decision.sign === 'UNDETERMINED' ? ' (assumed)' : '');

export function describeIdentification(id: CoordinateIdentification | null): string {
  if (id === null) {
    return 'Coordinate system not identified';
  }
  const { vertical: v, lateral: l, effective: e } = id;
  if (id.classification === 'MIXED') {
    return `Mixed: vertical ${SIDE[e.vertical]} (a loaded tire has FZ ${RELATION[e.vertical]} 0)${assumed(v)}, lateral ${SIDE[e.lateral]} (dFy/dSA ${RELATION[e.lateral]} 0)${assumed(l)}`;
  }
  return `${CLASSIFICATION_LABELS[id.classification]} (vertical ${SIDE[e.vertical]}, confidence ${v.confidence.toFixed(2)}${assumed(v)}; lateral ${SIDE[e.lateral]}, confidence ${l.confidence.toFixed(2)}${assumed(l)})`;
}

export interface ReadingRow {
  key: string;
  name: string;
  axis: IdentificationAxis;
  scale: string;
  value: string;
  status: ReadingStatus;
  vote: '+1' | '-1' | '0';
  weight: string;
}

function voteText(vote: number): '+1' | '-1' | '0' {
  if (vote > 0) {
    return '+1';
  }
  return vote < 0 ? '-1' : '0';
}

/** One table row per indicator reading, in the order of the identification. */
export function readingRows(id: CoordinateIdentification | null): ReadingRow[] {
  return (id?.readings ?? []).map((reading) => ({
    key: `${reading.name}@${reading.loadScale}`,
    name: reading.name,
    axis: reading.axis,
    scale: `${reading.loadScale}×`,
    value: Number.isFinite(reading.value) ? `${reading.value.toPrecision(4)} ${reading.unit}` : '—',
    status: reading.status,
    vote: voteText(reading.vote),
    weight: reading.weight.toFixed(3),
  }));
}

const negate = (value: number): number => 0 - value;

/** The sweeps after a VERTICAL conversion: every input negated by VERTICAL changes sign (a swept range stays ascending). LATERAL never touches a spec. */
export function convertSpec(spec: ChartSpec, inputs: readonly InputChannel[], applied: Transforms): ChartSpec {
  if (!applied.vertical) {
    return spec;
  }
  const flipped = new Set(inputs.filter((channel) => channel.negatedBy === 'VERTICAL').map((channel) => channel.name));
  const convertSweep = (sweep: SweepSpec): SweepSpec => ({
    ...sweep,
    swept: flipped.has(sweep.swept.variable)
      ? { ...sweep.swept, start: negate(sweep.swept.end), end: negate(sweep.swept.start) }
      : sweep.swept,
    steps: sweep.steps.map((step) =>
      flipped.has(step.variable) ? { ...step, values: step.values.map(negate) } : step,
    ),
    fixed: Object.fromEntries(
      Object.entries(sweep.fixed).map(([name, value]) => [name, flipped.has(name) ? negate(value) : value]),
    ),
  });
  return { ...spec, sweeps: spec.sweeps.map(convertSweep) };
}

const LISTED_CHANGES = 6;

export function describeConversion(target: CoordinateSystem, conversion: Conversion): string {
  if (!conversion.applied.vertical && !conversion.applied.lateral) {
    return `Already ${target}: no value changed sign`;
  }
  const n = conversion.changes.length;
  const keys = conversion.changes
    .slice(0, LISTED_CHANGES)
    .map((change) => change.key)
    .join(', ');
  return `Converted to ${target}: ${n} ${n === 1 ? 'value' : 'values'} changed sign (${keys}${n > LISTED_CHANGES ? ', …' : ''})`;
}

export function formatBatchWarning(sweep: SweepSpec, batch: SweepBatch, warning: BatchWarning): string {
  return `${warning.message} (first at ${describeBatchRow(sweep, batch, warning.row)})`;
}
