import { describe, expect, it } from 'vitest';
import { THEME_STORAGE_KEY, otherTheme, resolveTheme } from './theme';

describe('resolveTheme', () => {
  it('lets a stored choice beat the system preference', () => {
    expect(resolveTheme('light', true)).toBe('light');
    expect(resolveTheme('dark', false)).toBe('dark');
  });

  it('follows the system preference without a stored choice', () => {
    expect(resolveTheme(null, true)).toBe('dark');
    expect(resolveTheme(null, false)).toBe('light');
  });

  it('ignores a stored value that is not a theme', () => {
    expect(resolveTheme('garbage', true)).toBe('dark');
    expect(resolveTheme('garbage', false)).toBe('light');
  });
});

describe('otherTheme', () => {
  it('switches between light and dark', () => {
    expect(otherTheme('light')).toBe('dark');
    expect(otherTheme('dark')).toBe('light');
  });
});

describe('THEME_STORAGE_KEY', () => {
  it('is the persisted key', () => {
    expect(THEME_STORAGE_KEY).toBe('mf-theme');
  });
});
