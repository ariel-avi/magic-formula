import { CartesianGrid, Line, LineChart, ReferenceLine, XAxis, YAxis } from 'recharts';
import { ChartContainer, ChartTooltip, ChartTooltipContent } from '@/components/ui/chart';
import { formatSignificant } from '@/domain/numbers';
import type { ChartData } from '@/domain/series';
import { niceExtent, niceTicks } from '@/domain/ticks';

function extent(data: ChartData, keys: readonly string[]): [number, number] {
  let lo = Number.POSITIVE_INFINITY;
  let hi = Number.NEGATIVE_INFINITY;
  for (const row of data.rows) {
    for (const key of keys) {
      const v = row[key];
      if (Number.isFinite(v)) {
        lo = Math.min(lo, v);
        hi = Math.max(hi, v);
      }
    }
  }
  return Number.isFinite(lo) ? [lo, hi] : [0, 1];
}

export function SweepChart({ data }: { data: ChartData }) {
  const [xMin, xMax] = extent(data, ['x']);
  const yDomain = niceExtent(...extent(data, data.series.map((s) => s.key)), 8);
  return (
    <ChartContainer data-testid="sweep-chart" className="h-full min-h-[22rem] w-full">
      <LineChart data={data.rows} margin={{ top: 12, right: 20, bottom: 28, left: 12 }}>
        <CartesianGrid />
        <XAxis
          dataKey="x"
          type="number"
          domain={[xMin, xMax]}
          ticks={niceTicks(xMin, xMax, 8)}
          tickFormatter={(v: number) => formatSignificant(v, 4)}
          label={{ value: data.xLabel, position: 'insideBottom', offset: -16 }}
        />
        <YAxis
          width="auto"
          domain={yDomain}
          ticks={niceTicks(yDomain[0], yDomain[1], 6)}
          tickFormatter={(v: number) => formatSignificant(v, 4)}
          label={{ value: data.yLabel, angle: -90, position: 'insideLeft' }}
        />
        <ReferenceLine y={0} />
        <ReferenceLine x={0} />
        <ChartTooltip
          cursor={{ strokeWidth: 1 }}
          content={
            <ChartTooltipContent
              labelFormatter={(x) => `${data.xLabel} = ${formatSignificant(x, 5)}`}
              valueFormatter={(v) => formatSignificant(v, 5)}
            />
          }
        />
        {data.series.map((s) => (
          <Line
            key={s.key}
            dataKey={s.key}
            name={s.label}
            type="linear"
            stroke={s.colour}
            strokeWidth={2}
            dot={false}
            activeDot={{ r: 3, strokeWidth: 2 }}
            isAnimationActive={false}
          />
        ))}
      </LineChart>
    </ChartContainer>
  );
}
