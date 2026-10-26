import type { MfError } from './model';

/** The message, followed by its line number when the error has one. */
export function describeError(error: MfError): string {
  return error.line > 0 ? `${error.message} (line ${error.line})` : error.message;
}
