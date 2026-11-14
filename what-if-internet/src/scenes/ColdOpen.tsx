import React from 'react';
import {AbsoluteFill, useCurrentFrame} from 'remotion';
import {Stage} from '../components/three/Stage';
import {Globe, Stars} from '../components/three/Globe';
import {useLines, useScene} from '../lib/timeline';
import {Glitch, IconCard, Stat} from '../components/ui/Overlays';
import {prog, easeOut, windowFade} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

const LiveBadge: React.FC<{dead: number}> = ({dead}) => {
  const f = useCurrentFrame();
  const blink = Math.floor(f / 15) % 2 === 0 ? 1 : 0.35;
  const col = dead > 0.5 ? C.alert : C.data;
  const pts = Array.from({length: 40}, (_, i) => {
    const v = Math.sin((i + f * 0.6) * 0.5) * 0.4 + Math.sin((i + f * 0.6) * 0.17) * 0.6;
    const y = 30 - (dead > 0.5 && i > 20 ? 0 : v * 12 + 12);
    return `${i * 6},${y}`;
  }).join(' ');
  return (
    <div style={{position: 'absolute', left: 70, top: 64, display: 'flex', alignItems: 'center', gap: 18, opacity: prog(f, 20, 45)}}>
      <div style={{width: 14, height: 14, borderRadius: 7, background: col, opacity: blink, boxShadow: `0 0 14px ${col}`}} />
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, letterSpacing: 4, fontSize: 22, color: C.white}}>
        GLOBAL NETWORK · <span style={{color: col}}>{dead > 0.5 ? 'NO SIGNAL' : 'LIVE'}</span>
      </div>
      <svg width={240} height={34}>
        <polyline points={pts} fill="none" stroke={col} strokeWidth={2} />
      </svg>
    </div>
  );
};

const Countdown: React.FC<{shut: number; end: number}> = ({shut, end}) => {
  const f = useCurrentFrame();
  const start = shut - 90;
  if (f < start - 10) return null;
  const secsLeft = Math.max(0, Math.ceil((shut - f) / 30));
  const txt = secsLeft > 0 ? `23:59:${String(60 - secsLeft).padStart(2, '0')}` : '00:00:00';
  const dead = f >= shut;
  const o = prog(f, start - 10, start + 5, easeOut);
  return (
    <div style={{position: 'absolute', top: 150, left: 0, right: 0, textAlign: 'center', opacity: o}}>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 26, letterSpacing: 8, color: C.mist}}>UTC</div>
      <div
        style={{
          fontFamily: FONT_HEAD,
          fontWeight: 700,
          fontSize: 150,
          color: dead ? C.alert : C.white,
          textShadow: dead ? `0 0 60px ${C.alert}` : '0 0 40px rgba(63,224,255,0.4)',
          letterSpacing: 4,
          fontVariantNumeric: 'tabular-nums',
        }}
      >
        {txt}
      </div>
      <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 54, letterSpacing: 12, color: C.white, opacity: windowFade(f, end - 34, 99999, 14)}}>
        FOR <span style={{color: C.alert}}>24 HOURS</span>
      </div>
    </div>
  );
};

export const ColdOpen: React.FC = () => {
  const f = useCurrentFrame();
  const s = useScene();
  const L = useLines();
  const shut = L.c3.p[L.c3.p.length - 1] - 6; // on the word "stopped"
  const conn = 1 - prog(f, shut, shut + 18);
  const c2 = L.c2;
  const p = c2.p;
  return (
    <AbsoluteFill>
      <Stage
        bg="#040714"
        drift={0.03}
        cam={[
          {f: 0, pos: [0, 1.4, 13], target: [0, 0, 0], fov: 34},
          {f: c2.from - 20, pos: [1.2, 1.2, 7.6], target: [-0.2, 0, 0], fov: 36},
          {f: c2.from + 20, pos: [1.4, 1.0, 7.2], target: [-1.9, 0.15, 0], fov: 36},
          {f: L.c3.from, pos: [0.9, 0.8, 6.9], target: [-1.6, 0.1, 0], fov: 36},
          {f: L.c3.from + 30, pos: [0, 0.6, 7.4], target: [0, -0.9, 0], fov: 36},
          {f: s.duration, pos: [0, 0.4, 6.6], target: [0, -0.9, 0], fov: 36},
        ]}
      >
        <ambientLight intensity={0.25} />
        <directionalLight position={[-6, 3, 6]} intensity={1.6} color="#bcd7ff" />
        <Stars count={1800} />
        <Globe frame={f} rotation={-1.05 - f * 0.0048} connectivity={conn} cableDraw={prog(f, 10, 150)} landGlow={0.6 + 0.4 * conn} />
      </Stage>
      <LiveBadge dead={1 - conn} />
      <Stat value="5.5 billion" label="people online (about 68% of humanity)" at={c2.from + 4} until={L.c3.from} x={110} y={210} size={120} />
      <div style={{opacity: 1 - prog(f, L.c3.from - 10, L.c3.from + 2)}}>
        <div style={{position: 'absolute', left: 110, top: 560, display: 'flex', gap: 26}}>
          <IconCard icon="cash" label="Money" at={p[0] + 14} width={200} />
          <IconCard icon="hospital" label="Medicine" at={p[1] + 6} width={200} />
          <IconCard icon="map" label="Maps" at={p[2] + 6} width={200} />
          <IconCard icon="mail" label="Messages" at={p[2] + 24} width={200} />
        </div>
      </div>
      <Countdown shut={shut} end={L.c3.end} />
      <Glitch at={shut} len={18} />
      <AbsoluteFill style={{background: C.alert, mixBlendMode: 'multiply', opacity: 0.25 * prog(f, shut, shut + 10) * (1 - prog(f, shut + 10, shut + 40))}} />
      {/* sound design */}
      <Sfx name="riser" at={2} volume={0.25} />
      <Sfx name="pop" at={p[0] + 14} volume={0.35} />
      <Sfx name="pop" at={p[1] + 6} volume={0.35} />
      <Sfx name="pop" at={p[2] + 6} volume={0.35} />
      <Sfx name="pop" at={p[2] + 24} volume={0.35} />
      <Sfx name="tick" at={shut - 90} volume={0.6} />
      <Sfx name="tick" at={shut - 60} volume={0.6} />
      <Sfx name="tick" at={shut - 30} volume={0.6} />
      <Sfx name="powerDown" at={shut - 2} volume={0.7} />
      <Sfx name="glitch" at={shut} volume={0.5} />
      <Sfx name="boom" at={shut + 2} volume={0.8} />
    </AbsoluteFill>
  );
};
