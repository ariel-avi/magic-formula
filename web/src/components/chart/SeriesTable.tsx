import { cn } from '@/lib/utils';
import { ScrollArea, ScrollBar } from '@/components/ui/scroll-area';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table';
import { formatValue } from '@/domain/numbers';
import { tableColumns, type ChartData } from '@/domain/series';

export function SeriesTable({ data, grouped }: { data: ChartData; grouped: boolean }) {
  return (
    <ScrollArea className="h-[26rem] xl:h-full [&_[data-slot=table-container]]:overflow-visible">
      <div className="space-y-4">
        {data.groups.map((g) => {
          const columns = tableColumns(g, data.xLabel, data.yLabel);
          return (
            <section key={g.sweepId} data-testid="series-table-section" data-sweep={g.sweepId}>
              
              <Table
                data-testid="series-table"
                className={cn('font-mono text-xs tabular-nums slashed-zero', grouped && 'w-max')}
              >
                <TableHeader className="sticky top-0 bg-card">
                  {grouped && (
                    <TableRow>
                      <TableHead colSpan={columns.length} className="text-left font-semibold">
                        {g.name}
                      </TableHead>
                    </TableRow>
                  )}
                  <TableRow>
                    {columns.map((c) => (
                      <TableHead key={c.key} className="text-right">
                        {c.header}
                      </TableHead>
                    ))}
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {g.grid.map((_, i) => (
                    <TableRow key={i}>
                      {columns.map((c) => (
                        <TableCell key={c.key} className="text-right">
                          {formatValue(c.values[i] ?? NaN, 6)}
                        </TableCell>
                      ))}
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </section>
          );
        })}
      </div>
      <ScrollBar orientation="horizontal" />
    </ScrollArea>
  );
}
