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

export interface InputChannel {
  name: string;
  unit: string;
  minimum: number;
  minimumInclusive: boolean;
}

export interface OutputChannel {
  name: string;
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
  limits: LimitValue[];
}

export interface TirInspection {
  entries: TirEntry[];
  warnings: TirWarning[];
  ignoredKeys: string[];
  model: ModelInfo | null;
  loadError: MfError | null;
}
