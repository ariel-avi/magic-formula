import { DashSwatch } from '@/components/DashSwatch';
import { ScrollArea } from '@/components/ui/scroll-area';
import type { ChartData, Series } from '@/domain/series';

function LegendItem({ s }: { s: Series }) {
  return (
    <li
      data-testid="legend-item"
      data-colour={s.colour}
      data-dash={s.dash}
      className="flex items-center gap-1.5"
    >
      <DashSwatch colour={s.colour} dash={s.dash} />
      <span className="font-mono text-xs text-foreground">{s.label}</span>
    </li>
  );
}

export function SeriesLegend({ data, grouped }: { data: ChartData; grouped: boolean }) {
  if (!grouped) {
    const all = data.groups.flatMap((g) => g.series);
    if (all.length < 2) {
      return null;
    }
    return (
      <ScrollArea className="border-t [&>[data-slot=scroll-area-viewport]]:max-h-24">
        <ul data-testid="chart-legend" className="flex flex-wrap gap-x-4 gap-y-1 px-3 py-2">
          {all.map((s) => (
            <LegendItem key={s.key} s={s} />
          ))}
        </ul>
      </ScrollArea>
    );
  }
  return (
    <ScrollArea className="border-t xl:[&>[data-slot=scroll-area-viewport]]:max-h-40">
      <ul data-testid="chart-legend" className="flex flex-wrap gap-x-6 gap-y-2 px-3 py-2">
        {data.groups.map((g) => (
          <li
            key={g.sweepId}
            data-testid="legend-group"
            data-sweep={g.sweepId}
            className="flex flex-wrap items-center gap-x-4 gap-y-1"
          >
            <span data-testid="legend-group-name" className="flex items-center gap-1.5 text-xs font-medium">
              <DashSwatch colour="currentColor" dash={g.dash} />
              {g.name}
            </span>
            <ul className="contents">
              {g.series.map((s) => (
                <LegendItem key={s.key} s={s} />
              ))}
            </ul>
          </li>
        ))}
      </ul>
    </ScrollArea>
  );
}
