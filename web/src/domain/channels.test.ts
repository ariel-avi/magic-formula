import { describe, expect, it } from 'vitest';
import { channelLabel } from './channels';

describe('channelLabel', () => {
  it('labels a known input', () => {
    expect(channelLabel('FZ')).toEqual({ symbol: 'F', subscript: 'z', description: 'Vertical load' });
    expect(channelLabel('SA')).toEqual({ symbol: 'α', subscript: '', description: 'Slip angle' });
  });

  it('labels a known output', () => {
    expect(channelLabel('MZ')).toEqual({ symbol: 'M', subscript: 'z', description: 'Aligning moment' });
  });

  it('has no symbol for an unknown channel', () => {
    expect(channelLabel('FOO')).toEqual({ symbol: '', subscript: '', description: '' });
  });
});
