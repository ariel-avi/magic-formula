import * as React from 'react';
import * as RechartsPrimitive from 'recharts';
import type { TooltipContentProps } from 'recharts';

import { cn } from '@/lib/utils';

const CHART_CLASSES =
  'flex justify-center font-mono text-[11px] tabular-nums slashed-zero [&_.recharts-cartesian-axis-tick_text]:fill-muted-foreground [&_.recharts-cartesian-grid_line]:stroke-chart-grid [&_.recharts-cartesian-axis-line]:stroke-chart-axis [&_.recharts-cartesian-axis-tick-line]:stroke-chart-axis [&_.recharts-reference-line_line]:stroke-chart-axis [&_.recharts-label]:fill-foreground [&_.recharts-label]:font-sans [&_.recharts-label]:text-xs [&_.recharts-tooltip-cursor]:stroke-chart-cursor [&_.recharts-active-dot_.recharts-dot]:stroke-card [&_.recharts-surface]:outline-hidden';

function ChartContainer({
  className,
  children,
  ...props
}: React.ComponentProps<'div'> & {
  children: React.ComponentProps<typeof RechartsPrimitive.ResponsiveContainer>['children'];
}) {
  return (
    <div data-slot="chart" className={cn(CHART_CLASSES, className)} {...props}>
      <RechartsPrimitive.ResponsiveContainer width="100%" height="100%">
        {children}
      </RechartsPrimitive.ResponsiveContainer>
    </div>
  );
}

const ChartTooltip = RechartsPrimitive.Tooltip;

type ChartTooltipContentProps = Omit<
  Partial<TooltipContentProps<number, string>>,
  'labelFormatter' | 'formatter'
> & {
  className?: string;
  labelFormatter?: (label: number) => React.ReactNode;
  valueFormatter?: (value: number) => string;
};

function ChartTooltipContent({
  active,
  payload,
  label,
  className,
  labelFormatter,
  valueFormatter,
}: ChartTooltipContentProps) {
  if (!active || payload === undefined || payload.length === 0) {
    return null;
  }

  return (
    <div
      data-slot="chart-tooltip"
      className={cn(
        'grid min-w-40 gap-1.5 rounded-lg border bg-popover px-2.5 py-1.5 text-xs text-popover-foreground shadow-md',
        className,
      )}
    >
      <div data-slot="chart-tooltip-label" className="font-mono font-medium">
        {labelFormatter ? labelFormatter(Number(label)) : label}
      </div>
      <div className="grid gap-1">
        {payload.map((item, index) => (
          <div
            key={String(item.dataKey ?? item.name ?? index)}
            data-slot="chart-tooltip-item"
            className="flex items-center gap-2"
          >
            <span
              data-slot="chart-tooltip-key"
              className="h-0.5 w-2 shrink-0 rounded-full"
              style={{ backgroundColor: item.color ?? item.stroke }}
            />
            <span className="font-sans text-muted-foreground">{item.name}</span>
            <span className="ml-auto font-mono tabular-nums slashed-zero text-foreground">
              {valueFormatter && typeof item.value === 'number'
                ? valueFormatter(item.value)
                : String(item.value)}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

export { ChartContainer, ChartTooltip, ChartTooltipContent };
