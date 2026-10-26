import { ChevronDownIcon, DownloadIcon, FolderOpenIcon } from 'lucide-react';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu';
import { Separator } from '@/components/ui/separator';
import type { ModelInfo } from '@/domain/model';
import { EXAMPLES, type Example } from '@/examples';
import { ThemeToggle } from './ThemeToggle';

interface AppHeaderProps {
  solverReady: boolean;
  fileName: string | null;
  model: ModelInfo | null;
  canSave: boolean;
  onLoad: () => void;
  onOpenExample: (example: Example) => void;
  onSave: () => void;
}

export function AppHeader({
  solverReady,
  fileName,
  model,
  canSave,
  onLoad,
  onOpenExample,
  onSave,
}: AppHeaderProps): React.JSX.Element {
  return (
    <header
      data-testid="app-header"
      className="flex min-h-12 w-full flex-wrap items-center gap-x-3 gap-y-1 border-b bg-card px-3 py-1.5"
    >
      <h1 className="text-[15px] font-semibold whitespace-nowrap">Magic Formula Workbench</h1>
      {fileName !== null && <Separator orientation="vertical" className="h-5" />}
      {fileName !== null && (
        <span data-testid="file-name" className="min-w-0 truncate font-mono text-xs">
          {fileName}
        </span>
      )}
      {model !== null && (
        <Badge data-testid="model-name" variant="secondary" className="font-mono">
          {model.name}
          {model.fittyp !== null && ` (FITTYP ${model.fittyp})`}
        </Badge>
      )}
      <div className="ml-auto" />
      <div className="flex items-center gap-1.5">
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button variant="ghost" size="sm" data-testid="examples-menu" disabled={!solverReady}>
              Examples
              <ChevronDownIcon />
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent align="end">
            <DropdownMenuLabel>Example files</DropdownMenuLabel>
            {EXAMPLES.map((example) => (
              <DropdownMenuItem
                key={example.fileName}
                data-testid={`example-${example.fileName}`}
                className="font-mono text-xs"
                onSelect={() => onOpenExample(example)}
              >
                {example.fileName}
              </DropdownMenuItem>
            ))}
          </DropdownMenuContent>
        </DropdownMenu>
        <Button size="sm" data-testid="load-button" disabled={!solverReady} onClick={onLoad}>
          <FolderOpenIcon />
          Load .tir
        </Button>
        <Button size="sm" variant="outline" data-testid="save-button" disabled={!canSave} onClick={onSave}>
          <DownloadIcon />
          Save .tir
        </Button>
        <ThemeToggle />
      </div>
    </header>
  );
}
