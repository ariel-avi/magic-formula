import { ScrollArea, ScrollBar } from '@/components/ui/scroll-area';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table';
import { formatSignificant } from '@/domain/numbers';
import type { ChartData } from '@/domain/series';

export function SeriesTable({ data }: { data: ChartData }) {
  const labels = data.series.length > 1 ? data.series.map((s) => s.label) : [data.yLabel];
  return (
    <ScrollArea className="h-full [&_[data-slot=table-container]]:overflow-visible">
      <Table data-testid="series-table" className="font-mono text-xs tabular-nums slashed-zero">
        <TableHeader className="sticky top-0 bg-card">
          <TableRow>
            <TableHead className="text-right">{data.xLabel}</TableHead>
            {labels.map((label, index) => (
              <TableHead key={index} className="text-right">
                {label}
              </TableHead>
            ))}
          </TableRow>
        </TableHeader>
        <TableBody>
          {data.rows.map((row, index) => (
            <TableRow key={index}>
              <TableCell className="text-right">{formatSignificant(row['x'] ?? NaN, 6)}</TableCell>
              {data.series.map((s) => (
                <TableCell key={s.key} className="text-right">
                  {formatSignificant(row[s.key] ?? NaN, 6)}
                </TableCell>
              ))}
            </TableRow>
          ))}
        </TableBody>
      </Table>
      <ScrollBar orientation="horizontal" />
    </ScrollArea>
  );
}
