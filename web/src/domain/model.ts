export type ErrorCode =
  | 'IO'
  | 'SYNTAX'
  | 'INVALID_NUMBER'
  | 'DUPLICATE_KEY'
  | 'MISSING_KEY'
  | 'INVALID_VALUE'
  | 'UNSUPPORTED_UNIT'
  | 'UNSUPPORTED_MODEL'
  | 'NOT_IMPLEMENTED'
  | 'INVALID_INPUT'
  | 'NUMERICAL'
  | 'INVALID_ARGUMENT';

export interface MfError {
  code: ErrorCode;
  message: string;
  line: number;
  key: string;
}

export interface EvaluationError extends MfError {
  row: number | null;
}

export interface TirEntry {
  section: string;
  key: string;
  value: string;
  quoted: boolean;
  line: number;
  used: boolean;
}

export interface TirWarning {
  message: string;
  line: number;
  key: string;
}

export type CoordinateSystem = 'ISO' | 'SAE';

export type SignTransform = 'NONE' | 'VERTICAL' | 'LATERAL';

export interface InputChannel {
  name: string;
  unit: string;
  minimum: number;
  minimumInclusive: boolean;
  /** The transform that negates this input (FZ VERTICAL, SA LATERAL). */
  negatedBy: SignTransform;
}

export interface OutputChannel {
  name: string;
  unit: string;
  /** FY, MZ, MY: VERTICAL. */
  negatedBy: SignTransform;
}

/** A derived quantity of the evaluated Fx/Fy curves (layout order DFY_DSA ... SR_AT_PEAK_FX). */
export interface CharacteristicChannel {
  name: string;
  /** SI: 'N/rad' | 'N' | 'rad' | '-' */
  unit: string;
}

export interface LimitValue {
  key: string;
  role: 'LOWER' | 'UPPER' | 'VALUE';
  value: number;
}

export interface ModelInfo {
  name: string;
  fittyp: number | null;
  useMode: number | null;
  tyreSide: 'LEFT' | 'RIGHT';
  inputs: InputChannel[];
  outputs: OutputChannel[];
  /** Empty for a formulation without them. */
  characteristics: CharacteristicChannel[];
  limits: LimitValue[];
}

export type AxisSign = 'POSITIVE' | 'NEGATIVE' | 'UNDETERMINED';

export type DecisionBasis = 'DECIDED' | 'INSUFFICIENT_EVIDENCE' | 'CONFLICTING_EVIDENCE';

export type Classification = 'ISO' | 'SAE' | 'MIXED';

export type IdentificationAxis = 'VERTICAL' | 'LATERAL' | 'CHECK';

export type ReadingStatus = 'VOTE' | 'BELOW_THRESHOLD' | 'NOT_FINITE' | 'CLAMPED' | 'MISSING_CHANNEL';

export interface IndicatorReading {
  name: string;
  meaning: string;
  unit: string;
  axis: IdentificationAxis;
  loadScale: number;
  load: number;
  value: number;
  normalised: number;
  status: ReadingStatus;
  vote: number;
  weight: number;
}

export interface AxisDecision {
  sign: AxisSign;
  basis: DecisionBasis;
  confidence: number;
  positiveWeight: number;
  negativeWeight: number;
}

export interface CoordinateIdentification {
  classification: Classification;
  vertical: AxisDecision;
  lateral: AxisDecision;
  effective: { vertical: 'POSITIVE' | 'NEGATIVE'; lateral: 'POSITIVE' | 'NEGATIVE' };
  readings: IndicatorReading[];
  referenceInput: number[];
  warnings: TirWarning[];
}

export interface Transforms {
  vertical: boolean;
  lateral: boolean;
}

export interface TirChange {
  section: string;
  key: string;
  line: number;
  before: string;
  after: string;
}

export interface Conversion {
  text: string;
  applied: Transforms;
  changes: TirChange[];
}

export interface BatchWarning {
  message: string;
  key: string;
  row: number;
}

export interface SolvedBatch {
  outputs: Float64Array;
  /** rows x characteristics, column-major, SI; null when solved without them; NaN only in the two peak slips. */
  characteristics: Float64Array | null;
  warnings: BatchWarning[];
}

export interface TirInspection {
  entries: TirEntry[];
  warnings: TirWarning[];
  ignoredKeys: string[];
  model: ModelInfo | null;
  /** Null when model is null. */
  coordinates: CoordinateIdentification | null;
  loadError: MfError | null;
}
