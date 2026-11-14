import React from 'react';
import {AbsoluteFill, interpolate, useCurrentFrame, useVideoConfig} from 'remotion';
import {C, FONT_BODY, FONT_HEAD} from '../../theme';
import {easeIn, easeOut, pop, prog, windowFade} from '../../lib/anim';
import {Icon} from './Icons';

/* ---------- Chapter card: kicker + title, sweeps in then settles away ---------- */
export const ChapterCard: React.FC<{title: string; hold?: number}> = ({title, hold = 96}) => {
  const f = useCurrentFrame();
  const [kicker, main] = title.includes('—') ? title.split('—').map((s) => s.trim()) : ['Chapter', title];
  const inP = prog(f, 0, 18, easeOut);
  const outP = prog(f, hold, hold + 16, easeIn);
  const bar = prog(f, 4, 26, easeOut);
  return (
    <AbsoluteFill style={{pointerEvents: 'none'}}>
      <div
        style={{
          position: 'absolute',
          left: 120,
          bottom: 150,
          opacity: 1 - outP,
          transform: `translateY(${(1 - inP) * 40 - outP * 20}px)`,
        }}
      >
        <div style={{display: 'flex', alignItems: 'center', gap: 18, marginBottom: 14}}>
          <div style={{width: 16, height: 16, borderRadius: 4, background: C.data, boxShadow: `0 0 18px ${C.data}`}} />
          <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 30, letterSpacing: 6, color: C.data, textTransform: 'uppercase'}}>{kicker}</div>
        </div>
        <div
          style={{
            fontFamily: FONT_HEAD,
            fontWeight: 700,
            fontSize: 92,
            lineHeight: 1,
            color: C.white,
            letterSpacing: -1,
            textShadow: '0 6px 30px rgba(0,0,0,0.55)',
            clipPath: `inset(0 ${(1 - inP) * 100}% 0 0)`,
          }}
        >
          {main}
        </div>
        <div style={{height: 6, width: 260 * bar, background: `linear-gradient(90deg, ${C.data}, transparent)`, marginTop: 22, borderRadius: 3}} />
      </div>
    </AbsoluteFill>
  );
};

/* ---------- 24-hour progress clock (top-right) ---------- */
export const ClockHUD: React.FC<{hour: number; prevHour: number; visible: number}> = ({hour, prevHour, visible}) => {
  const f = useCurrentFrame();
  const h = interpolate(f, [10, 50], [prevHour, hour], {extrapolateLeft: 'clamp', extrapolateRight: 'clamp', easing: easeOut});
  const r = 46;
  const circ = 2 * Math.PI * r;
  const label = `${String(Math.floor(h)).padStart(2, '0')}:00`;
  return (
    <div
      style={{
        position: 'absolute',
        right: 70,
        top: 60,
        display: 'flex',
        alignItems: 'center',
        gap: 20,
        opacity: visible,
        padding: '14px 26px 14px 16px',
        background: 'rgba(7,11,30,0.55)',
        border: '1px solid rgba(169,181,217,0.25)',
        borderRadius: 70,
        backdropFilter: 'blur(6px)',
      }}
    >
      <svg width={110} height={110} viewBox="0 0 110 110">
        <circle cx={55} cy={55} r={r} stroke="rgba(255,255,255,0.12)" strokeWidth={8} fill="none" />
        {Array.from({length: 24}).map((_, i) => {
          const a = (i / 24) * Math.PI * 2 - Math.PI / 2;
          return <line key={i} x1={55 + Math.cos(a) * 34} y1={55 + Math.sin(a) * 34} x2={55 + Math.cos(a) * 30} y2={55 + Math.sin(a) * 30} stroke="rgba(255,255,255,0.35)" strokeWidth={1.5} />;
        })}
        <circle
          cx={55}
          cy={55}
          r={r}
          stroke={h >= 24 ? C.ok : C.alert}
          strokeWidth={8}
          fill="none"
          strokeLinecap="round"
          strokeDasharray={`${(circ * Math.min(24, Math.max(0.15, h))) / 24} ${circ}`}
          transform="rotate(-90 55 55)"
        />
      </svg>
      <div>
        <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 18, letterSpacing: 4, color: C.mist}}>OFFLINE FOR</div>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 52, color: C.white, lineHeight: 1.05}}>{label}</div>
      </div>
    </div>
  );
};

/* ---------- Source citation tag (bottom-left) ---------- */
export const SourceTag: React.FC<{text: string; dur: number}> = ({text, dur}) => {
  const f = useCurrentFrame();
  const o = windowFade(f, 6, dur, 12);
  return (
    <div
      style={{
        position: 'absolute',
        left: 60,
        bottom: 46,
        opacity: o * 0.92,
        fontFamily: FONT_BODY,
        fontSize: 22,
        color: C.mist,
        display: 'flex',
        alignItems: 'center',
        gap: 10,
        padding: '8px 16px',
        background: 'rgba(7,11,30,0.6)',
        borderRadius: 8,
        borderLeft: `3px solid ${C.data}`,
      }}
    >
      <span style={{fontWeight: 600, color: C.white, letterSpacing: 1}}>SOURCE</span>
      <span>{text}</span>
    </div>
  );
};

/* ---------- Big stat callout ---------- */
export const Stat: React.FC<{
  value: string;
  label: string;
  at?: number;
  until?: number;
  x?: number | string;
  y?: number | string;
  color?: string;
  align?: 'left' | 'center' | 'right';
  size?: number;
  icon?: string;
}> = ({value, label, at = 0, until = 99999, x = 120, y = 140, color = C.data, align = 'left', size = 140, icon}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  const o = Math.min(1, s) * (1 - prog(f, until - 12, until, easeIn));
  return (
    <div
      style={{
        position: 'absolute',
        left: x,
        top: y,
        opacity: o,
        textAlign: align,
        transform: `translate(${align === 'center' ? '-50%' : '0'}, ${(1 - s) * 30}px) scale(${0.9 + 0.1 * s})`,
        transformOrigin: 'left center',
      }}
    >
      <div style={{display: 'flex', alignItems: 'center', gap: 24, justifyContent: align === 'center' ? 'center' : 'flex-start'}}>
        {icon ? <Icon name={icon} size={size * 0.6} color={color} stroke={2} /> : null}
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: size, lineHeight: 0.95, color, textShadow: `0 0 40px ${color}55, 0 6px 26px rgba(0,0,0,0.6)`}}>{value}</div>
      </div>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: size * 0.24, color: C.white, marginTop: 12, textShadow: '0 3px 14px rgba(0,0,0,0.7)', maxWidth: 900}}>{label}</div>
    </div>
  );
};

/* ---------- Icon card (used for lists of affected / working systems) ---------- */
export type CardState = 'neutral' | 'down' | 'ok' | 'warn';
const stateColor = (s: CardState) => (s === 'down' ? C.alert : s === 'ok' ? C.ok : s === 'warn' ? C.amber : C.data);

export const IconCard: React.FC<{icon: string; label: string; state?: CardState; at?: number; stateAt?: number; width?: number}> = ({
  icon,
  label,
  state = 'neutral',
  at = 0,
  stateAt = 99999,
  width = 250,
}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  const flip = prog(f, stateAt, stateAt + 10);
  const cur: CardState = flip > 0.5 ? state : 'neutral';
  const col = stateColor(cur);
  return (
    <div
      style={{
        width,
        padding: '22px 18px 18px',
        borderRadius: 22,
        background: 'rgba(10,15,36,0.78)',
        border: `2px solid ${col}88`,
        boxShadow: `0 10px 40px rgba(0,0,0,0.45), inset 0 0 30px ${col}22`,
        opacity: Math.min(1, s),
        transform: `translateY(${(1 - s) * 40}px) scale(${0.85 + 0.15 * Math.min(1, s)})`,
        textAlign: 'center',
        position: 'relative',
      }}
    >
      <div style={{filter: cur === 'down' ? 'grayscale(0.6)' : undefined, opacity: cur === 'down' ? 0.55 : 1}}>
        <Icon name={icon} size={76} color={col} />
      </div>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 26, color: C.white, marginTop: 10}}>{label}</div>
      {cur !== 'neutral' ? (
        <div
          style={{
            position: 'absolute',
            top: -18,
            right: -18,
            width: 48,
            height: 48,
            borderRadius: 24,
            background: col,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            transform: `scale(${pop(f, fps, stateAt + 4, 9)})`,
            boxShadow: `0 0 20px ${col}`,
          }}
        >
          <Icon name={cur === 'down' ? 'x' : cur === 'ok' ? 'check' : 'alert'} size={30} color={C.ink} stroke={3} />
        </div>
      ) : null}
    </div>
  );
};

export const CardRow: React.FC<{children: React.ReactNode; y?: number; gap?: number}> = ({children, y = 640, gap = 36}) => (
  <div style={{position: 'absolute', left: 0, right: 0, top: y, display: 'flex', justifyContent: 'center', gap}}>{children}</div>
);

/* ---------- Small floating label (e.g. "GPS ✓") ---------- */
export const Tag: React.FC<{text: string; x: number; y: number; at?: number; until?: number; color?: string; icon?: string; size?: number}> = ({
  text,
  x,
  y,
  at = 0,
  until = 99999,
  color = C.data,
  icon,
  size = 30,
}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  const o = Math.min(1, s) * (1 - prog(f, until - 10, until));
  return (
    <div
      style={{
        position: 'absolute',
        left: x,
        top: y,
        transform: `translate(-50%, -50%) scale(${s})`,
        opacity: o,
        display: 'flex',
        alignItems: 'center',
        gap: 12,
        padding: '12px 22px',
        background: 'rgba(7,11,30,0.82)',
        border: `2px solid ${color}`,
        borderRadius: 40,
        fontFamily: FONT_BODY,
        fontWeight: 600,
        fontSize: size,
        color: C.white,
        whiteSpace: 'nowrap',
        boxShadow: `0 0 24px ${color}55`,
      }}
    >
      {icon ? <Icon name={icon} size={size * 1.25} color={color} stroke={2.4} /> : null}
      {text}
    </div>
  );
};

/* ---------- Headline text block ---------- */
export const Headline: React.FC<{text: string; sub?: string; at?: number; until?: number; x?: number; y?: number; size?: number; color?: string; center?: boolean}> = ({
  text,
  sub,
  at = 0,
  until = 99999,
  x = 120,
  y = 120,
  size = 76,
  color = C.white,
  center,
}) => {
  const f = useCurrentFrame();
  const p = prog(f, at, at + 16, easeOut);
  const o = p * (1 - prog(f, until - 12, until));
  return (
    <div style={{position: 'absolute', left: center ? 0 : x, right: center ? 0 : undefined, top: y, textAlign: center ? 'center' : 'left', opacity: o}}>
      <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: size, color, lineHeight: 1.05, transform: `translateY(${(1 - p) * 24}px)`, textShadow: '0 6px 30px rgba(0,0,0,0.6)'}}>{text}</div>
      {sub ? <div style={{fontFamily: FONT_BODY, fontWeight: 400, fontSize: size * 0.4, color: C.mist, marginTop: 14, textShadow: '0 3px 14px rgba(0,0,0,0.8)'}}>{sub}</div> : null}
    </div>
  );
};

/* ---------- Film finishing: vignette + animated grain ---------- */
export const Finish: React.FC<{grain?: number}> = ({grain = 0.06}) => {
  const f = useCurrentFrame();
  return (
    <AbsoluteFill style={{pointerEvents: 'none'}}>
      <AbsoluteFill style={{background: 'radial-gradient(ellipse at center, rgba(0,0,0,0) 55%, rgba(0,0,0,0.5) 100%)'}} />
      <svg width="100%" height="100%" style={{position: 'absolute', inset: 0, opacity: grain, mixBlendMode: 'overlay'}}>
        <filter id="grain">
          <feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves={2} seed={f % 24} stitchTiles="stitch" />
          <feColorMatrix type="saturate" values="0" />
        </filter>
        <rect width="100%" height="100%" filter="url(#grain)" />
      </svg>
    </AbsoluteFill>
  );
};

/* ---------- Scene transition: diagonal light-wipe centred on a cut ---------- */
export const Wipe: React.FC<{len?: number}> = ({len = 24}) => {
  const f = useCurrentFrame();
  const t = f / len; // 0..1, cut happens at 0.5
  const x = interpolate(t, [0, 0.5, 1], [-130, 0, 130], {easing: (v) => v});
  const cover = 1 - Math.abs(t - 0.5) * 2;
  return (
    <AbsoluteFill style={{pointerEvents: 'none', overflow: 'hidden'}}>
      <div
        style={{
          position: 'absolute',
          top: '-30%',
          left: `${x - 40}%`,
          width: '180%',
          height: '160%',
          transform: 'skewX(-18deg)',
          background: `linear-gradient(90deg, transparent 0%, ${C.data}55 18%, ${C.night} 30%, ${C.night} 70%, ${C.data}55 82%, transparent 100%)`,
          opacity: Math.min(1, cover * 2.2),
        }}
      />
    </AbsoluteFill>
  );
};

/* ---------- Digital glitch flash ---------- */
export const Glitch: React.FC<{at: number; len?: number; strength?: number}> = ({at, len = 14, strength = 1}) => {
  const f = useCurrentFrame() - at;
  if (f < 0 || f > len) return null;
  const k = (1 - f / len) * strength;
  return (
    <AbsoluteFill style={{pointerEvents: 'none'}}>
      {Array.from({length: 7}).map((_, i) => {
        const y = ((i * 137 + f * 53) % 100);
        const h = 2 + ((i * 7 + f * 3) % 9);
        return (
          <div
            key={i}
            style={{
              position: 'absolute',
              left: 0,
              right: 0,
              top: `${y}%`,
              height: `${h}%`,
              background: i % 2 ? `${C.alert}` : `${C.data}`,
              opacity: 0.18 * k,
              transform: `translateX(${((i * 31 + f * 17) % 40) - 20}px)`,
              mixBlendMode: 'screen',
            }}
          />
        );
      })}
      <AbsoluteFill style={{background: '#fff', opacity: f < 2 ? 0.25 * k : 0}} />
    </AbsoluteFill>
  );
};

/* ---------- Bubble above a character (screen-space) ---------- */
export const Bubble: React.FC<{x: number; y: number; icon: string; color?: string; at?: number; until?: number; spin?: boolean; text?: string}> = ({
  x,
  y,
  icon,
  color = C.white,
  at = 0,
  until = 99999,
  spin,
  text,
}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at, 10);
  const o = Math.min(1, s) * (1 - prog(f, until - 8, until));
  return (
    <div
      style={{
        position: 'absolute',
        left: x,
        top: y,
        transform: `translate(-50%, -100%) scale(${s})`,
        transformOrigin: 'bottom center',
        opacity: o,
        padding: text ? '12px 20px' : 14,
        borderRadius: 26,
        background: 'rgba(255,255,255,0.95)',
        boxShadow: '0 8px 24px rgba(0,0,0,0.35)',
        display: 'flex',
        alignItems: 'center',
        gap: 10,
        fontFamily: FONT_BODY,
        fontWeight: 600,
        fontSize: 26,
        color: C.ink,
      }}
    >
      <div style={{transform: spin ? `rotate(${f * 12}deg)` : undefined, display: 'flex'}}>
        <Icon name={icon} size={44} color={color} stroke={2.6} />
      </div>
      {text}
      <div style={{position: 'absolute', bottom: -10, left: '50%', marginLeft: -10, width: 0, height: 0, borderLeft: '10px solid transparent', borderRight: '10px solid transparent', borderTop: '12px solid rgba(255,255,255,0.95)'}} />
    </div>
  );
};

/* ---------- Quick fade from night at the start of a shot (soft cut) ---------- */
export const ShotIn: React.FC<{len?: number}> = ({len = 8}) => {
  const f = useCurrentFrame();
  const o = 1 - prog(f, 0, len);
  if (o <= 0) return null;
  return <AbsoluteFill style={{background: C.night, opacity: o, pointerEvents: 'none'}} />;
};
