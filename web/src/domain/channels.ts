export interface ChannelLabel {
  symbol: string;
  subscript: string;
  suffix: string;
  description: string;
}

const label = (symbol: string, subscript: string, suffix: string, description: string): ChannelLabel => ({
  symbol,
  subscript,
  suffix,
  description,
});

const LABELS: Readonly<Record<string, ChannelLabel>> = {
  FZ: label('F', 'z', '', 'Vertical load'),
  SA: label('α', '', '', 'Slip angle'),
  SR: label('κ', '', '', 'Slip ratio'),
  IA: label('γ', '', '', 'Inclination angle'),
  VX: label('V', 'x', '', 'Forward speed'),
  P: label('p', '', '', 'Inflation pressure'),
  FX: label('F', 'x', '', 'Longitudinal force'),
  FY: label('F', 'y', '', 'Lateral force'),
  MZ: label('M', 'z', '', 'Aligning moment'),
  MX: label('M', 'x', '', 'Overturning moment'),
  MY: label('M', 'y', '', 'Rolling resistance moment'),
  DFY_DSA: label('∂F', 'y', '/∂α', 'Instantaneous cornering stiffness'),
  DFX_DSR: label('∂F', 'x', '/∂κ', 'Instantaneous longitudinal stiffness'),
  FY_OVER_SA: label('F', 'y', '/α', 'Cornering stiffness (secant)'),
  FX_OVER_SR: label('F', 'x', '/κ', 'Longitudinal stiffness (secant)'),
  DY: label('D', 'y', '', 'Peak lateral force (pure-slip D)'),
  DX: label('D', 'x', '', 'Peak longitudinal force (pure-slip D)'),
  SA_AT_PEAK_FY: label('α', 'peak', '', 'Slip angle at peak lateral force'),
  SR_AT_PEAK_FX: label('κ', 'peak', '', 'Slip ratio at peak longitudinal force'),
};

/** Presentation label of a channel; an unknown channel has no symbol, so callers show its name only. */
export function channelLabel(name: string): ChannelLabel {
  return Object.hasOwn(LABELS, name) ? LABELS[name] : label('', '', '', '');
}
