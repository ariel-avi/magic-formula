import { describe, expect, it } from 'vitest';
import { describeError } from './errors';
import type { MfError } from './model';

const error = (line: number): MfError => ({ code: 'SYNTAX', message: 'Bad value', line, key: '' });

describe('describeError', () => {
  it('adds the line number when there is one', () => {
    expect(describeError(error(12))).toBe('Bad value (line 12)');
  });

  it('prints the bare message without a line number', () => {
    expect(describeError(error(0))).toBe('Bad value');
  });
});
