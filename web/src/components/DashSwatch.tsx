/** Wide enough to show one full period of the longest sweep dash pattern. */
const WIDTH = 32;

export function DashSwatch({ colour, dash }: { colour: string; dash: string }): React.JSX.Element {
  return (
    <svg aria-hidden="true" width={WIDTH} height={4} className="size-auto shrink-0">
      <line x1={0} y1={2} x2={WIDTH} y2={2} stroke={colour} strokeWidth={2} strokeDasharray={dash === '' ? undefined : dash} />
    </svg>
  );
}
