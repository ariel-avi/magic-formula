import { describe, expect, it } from 'vitest';
import { channelLabel } from './channels';

describe('channelLabel', () => {
  it('labels a known input', () => {
    expect(channelLabel('FZ')).toEqual({ symbol: 'F', subscript: 'z', suffix: '', description: 'Vertical load' });
    expect(channelLabel('SA')).toEqual({ symbol: 'α', subscript: '', suffix: '', description: 'Slip angle' });
  });

  it('labels a known output', () => {
    expect(channelLabel('MZ')).toEqual({ symbol: 'M', subscript: 'z', suffix: '', description: 'Aligning moment' });
  });

  it('has no symbol for an unknown channel', () => {
    expect(channelLabel('FOO')).toEqual({ symbol: '', subscript: '', suffix: '', description: '' });
  });

  it.each([
    ['DFY_DSA', '∂F', 'y', '/∂α', 'Instantaneous cornering stiffness'],
    ['DFX_DSR', '∂F', 'x', '/∂κ', 'Instantaneous longitudinal stiffness'],
    ['FY_OVER_SA', 'F', 'y', '/α', 'Cornering stiffness (secant)'],
    ['FX_OVER_SR', 'F', 'x', '/κ', 'Longitudinal stiffness (secant)'],
    ['DY', 'D', 'y', '', 'Peak lateral force (pure-slip D)'],
    ['DX', 'D', 'x', '', 'Peak longitudinal force (pure-slip D)'],
    ['SA_AT_PEAK_FY', 'α', 'peak', '', 'Slip angle at peak lateral force'],
    ['SR_AT_PEAK_FX', 'κ', 'peak', '', 'Slip ratio at peak longitudinal force'],
  ])('labels the characteristic %s', (name, symbol, subscript, suffix, description) => {
    expect(channelLabel(name)).toEqual({ symbol, subscript, suffix, description });
  });

  it('has an empty suffix for an existing channel', () => {
    expect(channelLabel('FY').suffix).toBe('');
  });
});
