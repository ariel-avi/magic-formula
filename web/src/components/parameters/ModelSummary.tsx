import { QuantitySymbol } from '@/components/QuantitySymbol';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { Badge } from '@/components/ui/badge';
import { Table, TableBody, TableCell, TableRow } from '@/components/ui/table';
import type { ModelInfo, TirWarning } from '@/domain/model';
import { formatLimit } from '@/domain/numbers';
import { displayUnit, hasUnit } from '@/domain/units';

interface ModelSummaryProps {
  model: ModelInfo | null;
  warnings: readonly TirWarning[];
}

function ChannelBadges({
  testId,
  itemPrefix,
  channels,
}: {
  testId: string;
  itemPrefix: string;
  channels: readonly { name: string; unit: string }[];
}) {
  return (
    <div data-testid={testId} className="flex flex-wrap gap-1">
      {channels.map(({ name, unit: siUnit }) => {
        const unit = displayUnit(siUnit).label;
        return (
          <Badge key={name} variant="outline" data-testid={`${itemPrefix}-${name}`} className="gap-1">
            <QuantitySymbol name={name} className="text-sm" />
            <span className="font-mono">{name}</span>
            {hasUnit(unit) && <span className="text-muted-foreground">[{unit}]</span>}
          </Badge>
        );
      })}
    </div>
  );
}

interface LimitRow {
  name: string;
  low: string;
  high: string;
}

/** One row per quantity (FZMIN/FZMAX -> FZ) with its lower and upper value side by side. */
function limitRows(limits: ModelInfo['limits']): LimitRow[] {
  const rows = new Map<string, LimitRow>();
  for (const limit of limits) {
    const name = limit.key.replace(/(MIN|MAX)$/, '');
    const row = rows.get(name) ?? { name, low: '—', high: '—' };
    if (limit.role === 'UPPER') {
      row.high = formatLimit(limit.value);
    } else {
      row.low = formatLimit(limit.value);
    }
    rows.set(name, row);
  }
  return [...rows.values()];
}

export function ModelSummary({ model, warnings }: ModelSummaryProps): React.JSX.Element {
  return (
    <section data-testid="model-summary" className="space-y-3 border-b px-3 py-3">
      <h2 className="text-[15px] font-semibold">Tire model</h2>
      {model === null ? (
        <p className="text-sm text-muted-foreground">No model</p>
      ) : (
        <dl className="grid grid-cols-[auto_1fr] items-baseline gap-x-4 gap-y-1.5 text-xs">
          <dt className="text-muted-foreground">Model</dt>
          <dd className="font-mono">{model.name}</dd>
          <dt className="text-muted-foreground">FITTYP</dt>
          <dd className="font-mono">{model.fittyp ?? '—'}</dd>
          <dt className="text-muted-foreground">USE_MODE</dt>
          <dd className="font-mono">{model.useMode ?? '—'}</dd>
          <dt className="text-muted-foreground">Tyre side</dt>
          <dd className="font-mono">{model.tyreSide}</dd>
          <dt className="text-muted-foreground">Inputs</dt>
          <dd>
            <ChannelBadges testId="model-inputs" itemPrefix="model-input" channels={model.inputs} />
          </dd>
          <dt className="text-muted-foreground">Outputs</dt>
          <dd>
            <ChannelBadges testId="model-outputs" itemPrefix="model-output" channels={model.outputs} />
          </dd>
          <dt className="text-muted-foreground">Limits</dt>
          <dd>
            {model.limits.length === 0 ? (
              <span className="text-muted-foreground">No limits</span>
            ) : (
              <Table data-testid="model-limits" className="text-xs">
                <TableBody>
                  {limitRows(model.limits).map((row) => (
                    <TableRow key={row.name} className="h-6">
                      <TableCell className="px-1 py-0.5 font-mono">{row.name}</TableCell>
                      <TableCell className="px-1 py-0.5 text-right font-mono tabular-nums slashed-zero">
                        {row.low}
                      </TableCell>
                      <TableCell className="px-1 py-0.5 text-right font-mono tabular-nums slashed-zero">
                        {row.high}
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            )}
          </dd>
        </dl>
      )}
      {warnings.length > 0 && (
        <Alert data-testid="model-warnings">
          <AlertTitle>Warnings</AlertTitle>
          <AlertDescription>
            <ul className="list-disc pl-4">
              {warnings.map((warning, index) => (
                <li key={`${index}/${warning.key}`}>
                  {warning.line > 0 ? `${warning.message} (line ${warning.line})` : warning.message}
                </li>
              ))}
            </ul>
          </AlertDescription>
        </Alert>
      )}
    </section>
  );
}
