export type RawErrorCode = 'IO' | 'SYNTAX' | 'INVALID_NUMBER' | 'DUPLICATE_KEY' | 'MISSING_KEY' | 'INVALID_VALUE'
  | 'UNSUPPORTED_UNIT' | 'UNSUPPORTED_MODEL' | 'NOT_IMPLEMENTED' | 'INVALID_INPUT' | 'NUMERICAL' | 'INVALID_ARGUMENT';
export interface RawError { code: RawErrorCode; message: string; line: number; key: string }
export interface RawWarning { message: string; line: number; key: string }
export interface RawEntry { section: string; key: string; value: string; quoted: boolean; line: number }
export interface RawInputChannel { name: string; unit: string; minimum: number; minimumInclusive: boolean }
export interface RawOutputChannel { name: string; unit: string }
export interface RawLimit { key: string; role: 'LOWER' | 'UPPER' | 'VALUE'; value: number }
export interface RawModel { name: string; fittyp: number | null; useMode: number | null; tyreSide: 'LEFT' | 'RIGHT';
  inputs: RawInputChannel[]; outputs: RawOutputChannel[]; limits: RawLimit[] }
export type RawInspection =
  | { ok: true; entries: RawEntry[]; warnings: RawWarning[]; ignoredKeys: string[]; model: RawModel | null;
      loadError: RawError | null }
  | { ok: false; error: RawError };
export type RawTextResult = { ok: true; text: string } | { ok: false; error: RawError };
export type RawEvaluation = { ok: true; rows: number; outputs: Float64Array }
  | { ok: false; error: RawError; row: number | null };
export interface MfWasmModule {
  version(): string;
  inspect(text: string, fileName: string): RawInspection;
  replaceValue(text: string, section: string, key: string, value: string): RawTextResult;
  evaluate(text: string, inputs: Float64Array, uncombined: boolean): RawEvaluation;
}
declare function createMfModule(moduleOverrides?: Record<string, unknown>): Promise<MfWasmModule>;
export default createMfModule;
