import {Easing, interpolate, random, spring} from 'remotion';

export const clamp = (v: number, a = 0, b = 1) => Math.min(b, Math.max(a, v));
export const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
export const easeInOut = Easing.bezier(0.65, 0, 0.35, 1);
export const easeOut = Easing.bezier(0.16, 1, 0.3, 1);
export const easeIn = Easing.bezier(0.7, 0, 0.84, 0);

/** 0→1 progress between frames a and b (eased, clamped). */
export const prog = (f: number, a: number, b: number, ease = easeInOut) =>
  interpolate(f, [a, b], [0, 1], {extrapolateLeft: 'clamp', extrapolateRight: 'clamp', easing: ease});

/** Opacity window: fades in at a, out at b. */
export const windowFade = (f: number, a: number, b: number, len = 10) =>
  Math.min(prog(f, a, a + len, easeOut), 1 - prog(f, b - len, b, easeIn));

export const pop = (f: number, fps: number, delay = 0, damping = 12, mass = 0.6) =>
  spring({frame: f - delay, fps, config: {damping, mass, stiffness: 140}});

/** Deterministic pseudo-random in [0,1). */
export const rnd = (seed: string | number) => random(String(seed));
export const rndRange = (seed: string | number, a: number, b: number) => a + rnd(seed) * (b - a);

/** Interpolate between keyframed vectors. keys: [frame, value[]] */
export const keyVec = (f: number, keys: [number, number[]][], ease = easeInOut): number[] => {
  if (f <= keys[0][0]) return keys[0][1];
  for (let i = 0; i < keys.length - 1; i++) {
    const [fa, va] = keys[i];
    const [fb, vb] = keys[i + 1];
    if (f <= fb) {
      const t = ease(clamp((f - fa) / Math.max(1, fb - fa)));
      return va.map((v, j) => lerp(v, vb[j], t));
    }
  }
  return keys[keys.length - 1][1];
};

export const mixColor = (a: string, b: string, t: number) => {
  const pa = parseInt(a.slice(1), 16);
  const pb = parseInt(b.slice(1), 16);
  const ch = (p: number, s: number) => (p >> s) & 255;
  const m = (s: number) => Math.round(lerp(ch(pa, s), ch(pb, s), clamp(t)));
  return '#' + ((1 << 24) + (m(16) << 16) + (m(8) << 8) + m(0)).toString(16).slice(1);
};

/** Numerically integrate a per-frame speed function from frame 0 to f (deterministic). */
export const integrate = (f: number, speed: (frame: number) => number) => {
  let acc = 0;
  for (let i = 0; i < Math.floor(f); i++) acc += speed(i);
  return acc + speed(Math.floor(f)) * (f - Math.floor(f));
};
