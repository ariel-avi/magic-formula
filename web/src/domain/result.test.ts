import { describe, expect, it } from 'vitest';
import { err, ok } from './result';

describe('result', () => {
  it('ok wraps a value', () => {
    expect(ok(3)).toEqual({ ok: true, value: 3 });
  });

  it('err wraps an error', () => {
    expect(err('bad')).toEqual({ ok: false, error: 'bad' });
  });
});
