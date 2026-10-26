export type RawErrorCode = 'IO' | 'SYNTAX' | 'INVALID_NUMBER' | 'DUPLICATE_KEY' | 'MISSING_KEY' | 'INVALID_VALUE'
  | 'UNSUPPORTED_UNIT' | 'UNSUPPORTED_MODEL' | 'NOT_IMPLEMENTED' | 'INVALID_INPUT' | 'NUMERICAL' | 'INVALID_ARGUMENT';
export interface RawError { code: RawErrorCode; message: string; line: number; key: string }
export interface RawWarning { message: string; line: number; key: string }
export type RawCoordinateSystem = 'ISO' | 'SAE';
export type RawSignTransform = 'NONE' | 'VERTICAL' | 'LATERAL';
export type RawAxisSign = 'POSITIVE' | 'NEGATIVE' | 'UNDETERMINED';
export type RawDecisionBasis = 'DECIDED' | 'INSUFFICIENT_EVIDENCE' | 'CONFLICTING_EVIDENCE';
export type RawClassification = 'ISO' | 'SAE' | 'MIXED';
export type RawIdentificationAxis = 'VERTICAL' | 'LATERAL' | 'CHECK';
export type RawReadingStatus = 'VOTE' | 'BELOW_THRESHOLD' | 'NOT_FINITE' | 'CLAMPED' | 'MISSING_CHANNEL';
export interface RawEntry { section: string; key: string; value: string; quoted: boolean; line: number }
export interface RawInputChannel { name: string; unit: string; minimum: number; minimumInclusive: boolean; negatedBy: RawSignTransform }
export interface RawOutputChannel { name: string; unit: string; negatedBy: RawSignTransform }
export interface RawCharacteristicChannel { name: string; unit: string }
export interface RawLimit { key: string; role: 'LOWER' | 'UPPER' | 'VALUE'; value: number }
export interface RawModel { name: string; fittyp: number | null; useMode: number | null; tyreSide: 'LEFT' | 'RIGHT';
  inputs: RawInputChannel[]; outputs: RawOutputChannel[];
  characteristics: RawCharacteristicChannel[]; limits: RawLimit[] }
export interface RawReading { name: string; meaning: string; unit: string; axis: RawIdentificationAxis; loadScale: number;
  load: number; value: number; normalised: number; status: RawReadingStatus; vote: number; weight: number }
export interface RawAxisDecision { sign: RawAxisSign; basis: RawDecisionBasis; confidence: number; positiveWeight: number;
  negativeWeight: number }
export interface RawIdentification {
  classification: RawClassification; vertical: RawAxisDecision; lateral: RawAxisDecision;
  effective: { vertical: 'POSITIVE' | 'NEGATIVE'; lateral: 'POSITIVE' | 'NEGATIVE' };
  readings: RawReading[]; referenceInput: number[]; warnings: RawWarning[];
}
export interface RawTransforms { vertical: boolean; lateral: boolean }
export interface RawTirChange { section: string; key: string; line: number; before: string; after: string }
export type RawConversion = { ok: true; text: string; applied: RawTransforms; changes: RawTirChange[] }
  | { ok: false; error: RawError };
export interface RawBatchWarning { message: string; key: string; row: number }
export type RawInspection =
  | { ok: true; entries: RawEntry[]; warnings: RawWarning[]; ignoredKeys: string[]; model: RawModel | null;
      coordinates: RawIdentification | null; loadError: RawError | null }
  | { ok: false; error: RawError };
export type RawTextResult = { ok: true; text: string } | { ok: false; error: RawError };
export type RawEvaluation = { ok: true; rows: number; outputs: Float64Array; warnings: RawBatchWarning[] }
  | { ok: false; error: RawError; row: number | null };
export type RawCharacteristicsEvaluation =
  | { ok: true; rows: number; outputs: Float64Array; characteristics: Float64Array; warnings: RawBatchWarning[] }
  | { ok: false; error: RawError; row: number | null };
export interface MfWasmModule {
  version(): string;
  inspect(text: string, fileName: string): RawInspection;
  replaceValue(text: string, section: string, key: string, value: string): RawTextResult;
  evaluate(text: string, inputs: Float64Array, uncombined: boolean): RawEvaluation;
  evaluateWithCharacteristics(text: string, inputs: Float64Array, uncombined: boolean): RawCharacteristicsEvaluation;
  convertModel(text: string, fileName: string, target: RawCoordinateSystem): RawConversion;
}
declare function createMfModule(moduleOverrides?: Record<string, unknown>): Promise<MfWasmModule>;
export default createMfModule;
