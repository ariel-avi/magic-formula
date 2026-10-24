export interface ChannelLabel {
  symbol: string;
  subscript: string;
  description: string;
}

const label = (symbol: string, subscript: string, description: string): ChannelLabel => ({
  symbol,
  subscript,
  description,
});

const LABELS: Readonly<Record<string, ChannelLabel>> = {
  FZ: label('F', 'z', 'Vertical load'),
  SA: label('α', '', 'Slip angle'),
  SR: label('κ', '', 'Slip ratio'),
  IA: label('γ', '', 'Inclination angle'),
  VX: label('V', 'x', 'Forward speed'),
  P: label('p', '', 'Inflation pressure'),
  FX: label('F', 'x', 'Longitudinal force'),
  FY: label('F', 'y', 'Lateral force'),
  MZ: label('M', 'z', 'Aligning moment'),
  MX: label('M', 'x', 'Overturning moment'),
  MY: label('M', 'y', 'Rolling resistance moment'),
};

/** Presentation label of a channel; an unknown channel has no symbol, so callers show its name only. */
export function channelLabel(name: string): ChannelLabel {
  return Object.hasOwn(LABELS, name) ? LABELS[name] : label('', '', '');
}
