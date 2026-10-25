import { CartesianGrid, Line, LineChart, ReferenceLine, XAxis, YAxis } from 'recharts';
import { ChartContainer, ChartTooltip, ChartTooltipContent } from '@/components/ui/chart';
import { formatSignificant } from '@/domain/numbers';
import type { ChartData } from '@/domain/series';

export function SweepChart({ data }: { data: ChartData }) {
  return (
    <ChartContainer data-testid="sweep-chart" className="h-full min-h-[22rem] w-full">
      <LineChart margin={{ top: 12, right: 20, bottom: 28, left: 12 }}>
        <CartesianGrid />
        <XAxis
          dataKey="x"
          type="number"
          domain={data.xDomain}
          ticks={data.xTicks}
          tickFormatter={(v: number) => formatSignificant(v, 4)}
          label={{ value: data.xLabel, position: 'insideBottom', offset: -16 }}
        />
        <YAxis
          width="auto"
          domain={data.yDomain}
          ticks={data.yTicks}
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
        {data.groups.flatMap((g) =>
          g.series.map((s) => (
            <Line
              key={s.key}
              data={s.points}
              dataKey="y"
              name={s.fullLabel}
              type="linear"
              stroke={s.colour}
              strokeDasharray={s.dash === '' ? undefined : s.dash}
              strokeWidth={2}
              dot={false}
              activeDot={{ r: 3, strokeWidth: 2 }}
              isAnimationActive={false}
            />
          )),
        )}
      </LineChart>
    </ChartContainer>
  );
}
