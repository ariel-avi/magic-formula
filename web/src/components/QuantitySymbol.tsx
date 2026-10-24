import { channelLabel } from '@/domain/channels';
import { cn } from '@/lib/utils';

export function QuantitySymbol({ name, className }: { name: string; className?: string }) {
  const { symbol, subscript } = channelLabel(name);
  if (symbol === '') {
    return null;
  }
  return (
    <span aria-hidden="true" className={cn('font-serif italic leading-none', className)}>
      {symbol}
      {subscript !== '' && <sub className="text-[0.65em]">{subscript}</sub>}
    </span>
  );
}
