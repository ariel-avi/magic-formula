import { lazy, Suspense } from 'react';
import { Alert, AlertDescription } from '@/components/ui/alert';
import { Collapsible, CollapsibleContent, CollapsibleTrigger } from '@/components/ui/collapsible';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table';
import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group';
import {
  activeSystem,
  CLASSIFICATION_LABELS,
  describeIdentification,
  readingRows,
  sceneSystem,
} from '@/domain/coordinates';
import type { CoordinateIdentification, CoordinateSystem } from '@/domain/model';

const TireScene = lazy(() => import('./TireScene'));

interface CoordinateSystemPanelProps {
  identification: CoordinateIdentification | null;
  conversionNote: string | null;
  onConvert: (target: CoordinateSystem) => void;
}

export function CoordinateSystemPanel({
  identification,
  conversionNote,
  onConvert,
}: CoordinateSystemPanelProps): React.JSX.Element {
  const warnings = identification?.warnings ?? [];
  const active = activeSystem(identification);
  const rows = readingRows(identification);
  return (
    <Collapsible defaultOpen asChild>
      <section data-testid="coordinate-panel" className="border-b px-3 py-3">
        <div className="flex items-center justify-between">
          <h2 className="text-[15px] font-semibold">Coordinate system</h2>
          <CollapsibleTrigger
            data-testid="coordinate-panel-toggle"
            className="text-xs text-muted-foreground underline-offset-2 hover:underline"
          >
            Show/hide
          </CollapsibleTrigger>
        </div>
        <CollapsibleContent className="mt-3 space-y-3">
          <div className="flex items-center gap-3">
            <ToggleGroup
              type="single"
              variant="outline"
              size="sm"
              value={active ?? ''}
              onValueChange={(value) => {
                if ((value === 'ISO' || value === 'SAE') && value !== active) {
                  onConvert(value);
                }
              }}
            >
              <ToggleGroupItem value="ISO" data-testid="coordinate-system-ISO">
                ISO
              </ToggleGroupItem>
              <ToggleGroupItem value="SAE" data-testid="coordinate-system-SAE">
                SAE
              </ToggleGroupItem>
            </ToggleGroup>
            <span data-testid="coordinate-system-label" className="text-sm font-medium">
              {identification === null ? 'Not identified' : CLASSIFICATION_LABELS[identification.classification]}
            </span>
          </div>
          <Suspense fallback={<div data-testid="tire-scene-loading" className="h-64" />}>
            <TireScene system={sceneSystem(identification)} />
          </Suspense>
          <p data-testid="coordinate-identification" className="text-xs text-muted-foreground">
            {describeIdentification(identification)}
          </p>
          {conversionNote !== null && (
            <p data-testid="coordinate-conversion-note" className="text-xs">
              {conversionNote}
            </p>
          )}
          {warnings.length > 0 && (
            <Alert data-testid="coordinate-warnings">
              <AlertDescription>
                <ul className="list-disc pl-4">
                  {warnings.map((warning, index) => (
                    <li key={`${index}/${warning.message}`}>{warning.message}</li>
                  ))}
                </ul>
              </AlertDescription>
            </Alert>
          )}
          <Collapsible data-testid="coordinate-indicators">
            <CollapsibleTrigger className="text-xs text-muted-foreground underline-offset-2 hover:underline">
              {`Indicators (${rows.length})`}
            </CollapsibleTrigger>
            <CollapsibleContent className="mt-2 overflow-x-auto">
              <Table className="text-xs">
                <TableHeader>
                  <TableRow>
                    <TableHead>Indicator</TableHead>
                    <TableHead>Axis</TableHead>
                    <TableHead>Load</TableHead>
                    <TableHead>Value</TableHead>
                    <TableHead>Status</TableHead>
                    <TableHead>Vote</TableHead>
                    <TableHead>Weight</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {rows.map((row) => (
                    <TableRow key={row.key} data-testid="coordinate-indicator-row">
                      <TableCell>{row.name}</TableCell>
                      <TableCell>{row.axis}</TableCell>
                      <TableCell>{row.scale}</TableCell>
                      <TableCell>{row.value}</TableCell>
                      <TableCell>{row.status}</TableCell>
                      <TableCell>{row.vote}</TableCell>
                      <TableCell>{row.weight}</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </CollapsibleContent>
          </Collapsible>
        </CollapsibleContent>
      </section>
    </Collapsible>
  );
}
