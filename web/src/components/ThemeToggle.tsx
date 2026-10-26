import { MoonIcon, SunIcon } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { otherTheme } from '@/domain/theme';
import { useTheme } from './ThemeProvider';

export function ThemeToggle(): React.JSX.Element {
  const { theme, toggleTheme } = useTheme();
  return (
    <Button
      variant="ghost"
      size="icon"
      className="size-8"
      data-testid="theme-toggle"
      aria-label={`Use ${otherTheme(theme)} theme`}
      onClick={toggleTheme}
    >
      {theme === 'dark' ? <SunIcon /> : <MoonIcon />}
    </Button>
  );
}
