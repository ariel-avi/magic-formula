import { Button } from '@/components/ui/button';
import { EXAMPLES, type Example } from '@/examples';

interface EmptyStateProps {
  solverReady: boolean;
  onLoad: () => void;
  onOpenExample: (example: Example) => void;
}

export function EmptyState({ solverReady, onLoad, onOpenExample }: EmptyStateProps): React.JSX.Element {
  return (
    <main data-testid="empty-state" className="flex w-full flex-1 flex-col items-start gap-6 px-6 py-16 sm:px-12">
      <p className="font-serif text-[28px] leading-tight italic sm:text-[32px]">
        y = D sin(C arctan(Bx − E(Bx − arctan Bx)))
      </p>
      <p className="text-sm text-muted-foreground">Open a tire property file (.tir) to plot its forces and moments.</p>
      <div className="flex flex-wrap gap-2">
        <Button data-testid="empty-load" disabled={!solverReady} onClick={onLoad}>
          Load .tir
        </Button>
        {EXAMPLES.map((example) => (
          <Button
            key={example.fileName}
            variant="outline"
            data-testid={`open-example-${example.fileName}`}
            disabled={!solverReady}
            onClick={() => onOpenExample(example)}
          >
            Open example {example.fileName}
          </Button>
        ))}
      </div>
      {!solverReady && (
        <p data-testid="solver-loading" className="text-xs text-muted-foreground">
          Loading solver…
        </p>
      )}
    </main>
  );
}
