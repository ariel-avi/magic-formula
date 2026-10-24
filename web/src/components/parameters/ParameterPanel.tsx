import { useMemo, useState } from 'react';
import { ChevronRightIcon } from 'lucide-react';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Collapsible, CollapsibleContent, CollapsibleTrigger } from '@/components/ui/collapsible';
import { Input } from '@/components/ui/input';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Table, TableBody, TableCell, TableRow } from '@/components/ui/table';
import { changedEntryIds, filterGroups, groupBySection } from '@/domain/entries';
import type { TirEntry } from '@/domain/model';
import { EntryField } from './EntryField';

interface ParameterPanelProps {
  original: readonly TirEntry[];
  entries: readonly TirEntry[];
  /** Returns an error message, or null when the edit was applied. */
  onEdit: (entry: TirEntry, value: string) => string | null;
}

export function ParameterPanel({ original, entries, onEdit }: ParameterPanelProps): React.JSX.Element {
  const [filter, setFilter] = useState('');
  const [closed, setClosed] = useState<ReadonlySet<string>>(new Set());
  const changed = useMemo(() => changedEntryIds(original, entries), [original, entries]);
  const groups = useMemo(() => filterGroups(groupBySection(entries), filter), [entries, filter]);

  const setOpen = (section: string, open: boolean) => {
    setClosed((previous) => {
      const next = new Set(previous);
      if (open) {
        next.delete(section);
      } else {
        next.add(section);
      }
      return next;
    });
  };

  return (
    <section data-testid="parameter-panel" aria-label="Parameters" className="flex min-h-0 flex-col xl:flex-1">
      <div className="flex items-center justify-between gap-3 border-b px-3 py-2">
        <h2 className="text-[15px] font-semibold">Parameters</h2>
        <Input
          aria-label="Filter parameters"
          data-testid="parameter-filter"
          placeholder="Filter, e.g. PCY1"
          className="h-7 w-44 font-mono text-xs md:text-xs"
          value={filter}
          onChange={(event) => setFilter(event.target.value)}
        />
      </div>
      <ScrollArea className="h-[36rem] xl:h-auto xl:min-h-0 xl:flex-1">
        {groups.length === 0 ? (
          <p data-testid="parameter-empty" className="px-3 py-4 text-xs text-muted-foreground">
            No parameter matches this filter.
          </p>
        ) : (
          groups.map(({ section, entries: rows }) => (
            <Collapsible
              key={section}
              data-testid={`param-section-${section}`}
              open={filter.trim() !== '' || !closed.has(section)}
              onOpenChange={(open) => setOpen(section, open)}
            >
              <CollapsibleTrigger asChild>
                <Button
                  variant="ghost"
                  size="sm"
                  className="sticky top-0 z-10 h-7 w-full justify-start gap-2 rounded-none bg-card font-mono text-xs"
                >
                  <ChevronRightIcon className="transition-transform [[data-state=open]_&]:rotate-90" />[{section}]
                  <Badge variant="secondary" className="ml-auto font-mono">
                    {rows.length}
                  </Badge>
                </Button>
              </CollapsibleTrigger>
              <CollapsibleContent>
                <Table className="table-fixed text-xs">
                  <TableBody>
                    {rows.map((entry) => (
                      <TableRow
                        key={`${entry.section}/${entry.key}/${entry.value}`}
                        data-testid={`param-row-${section}-${entry.key}`}
                      >
                        <TableCell className={entry.used ? 'w-36 font-mono' : 'w-36 font-mono text-muted-foreground'}>
                          <div>{entry.key}</div>
                          {!entry.used && (
                            <Badge variant="outline" className="mt-0.5">
                              unused
                            </Badge>
                          )}
                        </TableCell>
                        <TableCell>
                          <EntryField entry={entry} onEdit={onEdit} />
                        </TableCell>
                        <TableCell className="w-16 px-1">
                          {changed.has(`${entry.section}/${entry.key}`) && (
                            <Badge variant="secondary" data-testid={`edited-${section}-${entry.key}`}>
                              edited
                            </Badge>
                          )}
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </CollapsibleContent>
            </Collapsible>
          ))
        )}
      </ScrollArea>
    </section>
  );
}
