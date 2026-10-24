export type Theme = 'light' | 'dark';

export const THEME_STORAGE_KEY = 'mf-theme';

/** A stored 'light' or 'dark' wins; anything else (null or garbage) follows the system preference. */
export function resolveTheme(stored: string | null, prefersDark: boolean): Theme {
  if (stored === 'light' || stored === 'dark') {
    return stored;
  }
  return prefersDark ? 'dark' : 'light';
}

export function otherTheme(theme: Theme): Theme {
  return theme === 'light' ? 'dark' : 'light';
}
