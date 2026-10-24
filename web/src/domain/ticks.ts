/** Rounds away floating-point noise (0.30000000000000004 -> 0.3) and normalises -0 to 0. */
function clean(value: number): number {
  return Number(value.toPrecision(12)) + 0;
}

/** A 1, 2 or 5 times a power of ten that is at least `raw` (raw must be positive). */
function niceStep(raw: number): number {
  const exponent = Math.floor(Math.log10(raw));
  const fraction = raw / 10 ** exponent;
  let nice = 10;
  if (fraction <= 1) {
    nice = 1;
  } else if (fraction <= 2) {
    nice = 2;
  } else if (fraction <= 5) {
    nice = 5;
  }
  return nice * 10 ** exponent;
}

/** A 1, 2, 2.5 or 5 times a power of ten that is at least `raw`; finer than niceStep so extents hug the data. */
function fineStep(raw: number): number {
  const exponent = Math.floor(Math.log10(raw));
  const fraction = raw / 10 ** exponent;
  const nice = [1, 2, 2.5, 5].find((candidate) => fraction <= candidate) ?? 10;
  return nice * 10 ** exponent;
}

/** Round tick values (multiples of 1, 2 or 5 times a power of ten) inside [min, max], about `target` of them. Zero is included whenever it lies in the range. */
export function niceTicks(min: number, max: number, target: number): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max)) {
    return [];
  }
  if (!(max > min)) {
    return [clean(min)];
  }
  const step = niceStep((max - min) / target);
  const first = Math.ceil(min / step - 1e-9);
  const last = Math.floor(max / step + 1e-9);
  const ticks: number[] = [];
  for (let i = first; i <= last; i += 1) {
    ticks.push(clean(i * step));
  }
  return ticks;
}

/** Smallest range with round ends that contains [min, max], for an axis that should end on a tick. */
export function niceExtent(min: number, max: number, target: number): [number, number] {
  if (!(max > min)) {
    return [clean(min - 1), clean(max + 1)];
  }
  const step = fineStep((max - min) / target);
  return [clean(Math.floor(min / step + 1e-9) * step), clean(Math.ceil(max / step - 1e-9) * step)];
}
