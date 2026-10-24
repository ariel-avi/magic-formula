import { ScrollArea } from '@/components/ui/scroll-area';
import type { Series } from '@/domain/series';

export function SeriesLegend({ series }: { series: readonly Series[] }) {
  if (series.length < 2) {
    return null;
  }
  return (
    <ScrollArea className="border-t [&>[data-slot=scroll-area-viewport]]:max-h-24">
      <ul data-testid="chart-legend" className="flex flex-wrap gap-x-4 gap-y-1 px-3 py-2">
        {series.map((s) => (
          <li key={s.key} data-testid="legend-item" data-colour={s.colour} className="flex items-center gap-1.5">
            <span
              aria-hidden="true"
              className="h-0.5 w-4 shrink-0 rounded-full"
              style={{ backgroundColor: s.colour }}
            />
            <span className="font-mono text-xs text-foreground">{s.label}</span>
          </li>
        ))}
      </ul>
    </ScrollArea>
  );
}
