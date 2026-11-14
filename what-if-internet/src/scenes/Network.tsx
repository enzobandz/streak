import React, {useMemo} from 'react';
import {AbsoluteFill, Sequence, useCurrentFrame, useVideoConfig} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {Globe, Stars} from '../components/three/Globe';
import {BigSwitch} from '../components/three/Props';
import {buildGraph, NetworkGraph, Seabed} from '../components/three/Network';
import {useLines, useScene} from '../lib/timeline';
import {ShotIn, Stat, Tag} from '../components/ui/Overlays';
import {easeOut, pop, prog} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

const Studio: React.FC<{color?: string}> = ({color = '#141c3d'}) => (
  <>
    <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
      <circleGeometry args={[40, 64]} />
      <meshStandardMaterial color={color} roughness={0.9} />
    </mesh>
    <gridHelper args={[60, 60, '#2a3a74', '#1d2856']} position={[0, 0.01, 0]} />
  </>
);

const ShotSwitch: React.FC<{stampAt: number; dur: number}> = ({stampAt, dur}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const jiggle = f > stampAt - 30 && f < stampAt ? Math.sin(f * 1.3) * 0.12 : 0;
  const s = pop(f, fps, stampAt, 10);
  return (
    <AbsoluteFill>
      <Stage bg="#0b1129" fog={['#0b1129', 14, 40]} shadows cam={[{f: 0, pos: [5, 3.2, 9], target: [0, 1.8, 0], fov: 38}, {f: dur, pos: [2.2, 2.4, 6.2], target: [0, 1.8, 0], fov: 38}]}>
        <Lights intensity={0.8} shadow shadowSize={8} sunPos={[4, 10, 6]} />
        <spotLight position={[0, 9, 4]} angle={0.5} penumbra={0.6} intensity={60} color="#bfe9ff" castShadow />
        <Studio />
        <BigSwitch position={[0, 1.9, 0]} on={1 - jiggle} scale={1} />
      </Stage>
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>
        <div
          style={{
            transform: `rotate(-8deg) scale(${1.6 - 0.6 * Math.min(1, s)})`,
            opacity: Math.min(1, s),
            border: `10px solid ${C.alert}`,
            borderRadius: 18,
            padding: '18px 44px',
            fontFamily: FONT_HEAD,
            fontWeight: 700,
            fontSize: 110,
            color: C.alert,
            letterSpacing: 6,
            background: 'rgba(11,17,41,0.55)',
            textShadow: `0 0 30px ${C.alert}66`,
          }}
        >
          NO OFF SWITCH
        </div>
      </AbsoluteFill>
      <Sfx name="switch" at={stampAt - 28} volume={0.5} />
      <Sfx name="switch" at={stampAt - 14} volume={0.5} />
      <Sfx name="hit" at={stampAt} volume={0.45} />
    </AbsoluteFill>
  );
};

const ShotGraph: React.FC<{L: ReturnType<typeof useLines>; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const graph = useMemo(() => buildGraph('net', 9, 9, 7.5), []);
  const n2 = L.n2.from - base, n3 = L.n3.from - base;
  const p2 = L.n2.p.map((x) => x - base);
  const p3 = L.n3.p.map((x) => x - base);
  const per = 9;
  const route = [3, 0, -1, 4 * per, 4 * per + 5];
  const legend = [
    {t: 'Phone companies', c: C.data, at: p2[1] + 6},
    {t: 'Universities', c: C.amber, at: p2[2]},
    {t: 'Cloud providers', c: C.violet, at: p2[3]},
    {t: 'Governments', c: C.ok, at: p2[3] + 32},
  ];
  return (
    <AbsoluteFill>
      <Stage
        bg="#080d24"
        fog={['#080d24', 18, 46]}
        cam={[
          {f: 0, pos: [3, 4, 26], target: [-5, 0, 0], fov: 40},
          {f: n3 - 10, pos: [14, 6, 18], target: [-4, 0, 0], fov: 40},
          {f: n3 + 30, pos: [9, 4, 12], target: [0, 0, 0], fov: 40},
          {f: dur, pos: [-6, 5, 13], target: [0, -0.5, 0], fov: 40},
        ]}
      >
        <ambientLight intensity={0.5} />
        <pointLight position={[0, 6, 6]} intensity={40} color="#9fd3ff" />
        <Stars count={700} opacity={0.5} />
        <NetworkGraph
          frame={f}
          graph={graph}
          reveal={prog(f, 0, p2[1], easeOut)}
          linkReveal={prog(f, 20, p2[1] + 30)}
          exchange={prog(f, n3 + 10, n3 + 40, easeOut)}
          route={route}
          routeProgress={prog(f, p3[0] + 10, p3[1] + 20)}
        />
      </Stage>
      <Stat value="70,000+" label="independent networks" at={p2[0]} until={n3 + 6} x={110} y={120} size={130} />
      <div style={{position: 'absolute', left: 110, top: 420, display: 'flex', flexDirection: 'column', gap: 18, opacity: 1 - prog(f, n3 - 6, n3 + 6)}}>
        {legend.map((l) => (
          <LegendRow key={l.t} text={l.t} color={l.c} at={l.at} />
        ))}
      </div>
      <Tag text="Internet Exchange Point" icon="network" x={960} y={170} at={n3 + 20} until={p3[0] + 10} color={C.data} />
      <Tag text="BGP · the shared map of routes" icon="map" x={960} y={170} at={p3[0] + 14} until={dur} color={C.amber} />
      {legend.map((l) => (
        <Sfx key={l.t} name="pop" at={l.at} volume={0.3} />
      ))}
      <ShotIn />
      <Sfx name="whooshShort" at={n3 + 10} volume={0.4} />
      <Sfx name="blip" at={p3[0] + 14} volume={0.4} />
      <Sfx name="riser" at={p3[0] + 20} volume={0.2} />
    </AbsoluteFill>
  );
};

const LegendRow: React.FC<{text: string; color: string; at: number}> = ({text, color, at}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  return (
    <div style={{display: 'flex', alignItems: 'center', gap: 18, opacity: Math.min(1, s), transform: `translateX(${(1 - s) * -40}px)`}}>
      <div style={{width: 26, height: 26, borderRadius: 13, background: color, boxShadow: `0 0 18px ${color}`}} />
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 38, color: C.white, textShadow: '0 3px 12px #000'}}>{text}</div>
    </div>
  );
};

/** Garden-hose cross-section: layered cable diagram. */
const CableSection: React.FC<{at: number}> = ({at}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at, 14);
  const layers = [
    {r: 100, c: '#1a1f2e', l: 'Polyethylene sheath'},
    {r: 82, c: '#c27c3a', l: 'Copper (carries power for repeaters)'},
    {r: 64, c: '#9aa3bd', l: 'Steel strength wires'},
    {r: 40, c: '#e8b34a', l: 'Protective tube & gel'},
    {r: 18, c: C.data, l: 'Optical fibres'},
  ];
  return (
    <div style={{position: 'absolute', right: 110, top: 200, opacity: Math.min(1, s), transform: `scale(${0.8 + 0.2 * Math.min(1, s)})`, transformOrigin: 'right top', display: 'flex', gap: 34, alignItems: 'center', background: 'rgba(7,11,30,0.75)', padding: 34, borderRadius: 28, border: `2px solid ${C.data}55`}}>
      <svg width={230} height={230} viewBox="-115 -115 230 230">
        {layers.map((l, i) => (
          <circle key={i} r={l.r * Math.min(1, s * (1 + i * 0.1))} fill={l.c} stroke="#0a0f24" strokeWidth={2} />
        ))}
        {Array.from({length: 6}).map((_, i) => (
          <circle key={i} cx={Math.cos((i / 6) * Math.PI * 2) * 8} cy={Math.sin((i / 6) * Math.PI * 2) * 8} r={4} fill="#eafcff" />
        ))}
      </svg>
      <div style={{fontFamily: FONT_BODY, color: C.white}}>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 44, marginBottom: 14}}>≈ a garden hose</div>
        {layers.map((l, i) => (
          <div key={i} style={{display: 'flex', alignItems: 'center', gap: 12, fontSize: 24, marginTop: 8, opacity: prog(f, at + 10 + i * 6, at + 22 + i * 6)}}>
            <div style={{width: 16, height: 16, borderRadius: 8, background: l.c, border: '1px solid #ffffff55'}} />
            {l.l}
          </div>
        ))}
        <div style={{fontSize: 20, color: C.mist, marginTop: 16}}>Deep-ocean section · simplified</div>
      </div>
    </div>
  );
};

const ShotSeabed: React.FC<{L: ReturnType<typeof useLines>; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p4 = L.n4.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage
        bg="#0b3550"
        fog={['#0b3550', 6, 34]}
        shadows
        cam={[
          {f: 0, pos: [3, 3.2, 38], target: [0, 0.5, 26], fov: 45},
          {f: dur, pos: [-1.8, 1.7, 14], target: [0.5, 0.4, 2], fov: 45},
        ]}
      >
        <hemisphereLight args={['#8fd6ff', '#0b2233', 1.2]} />
        <directionalLight position={[2, 20, 4]} intensity={1.6} color="#bfefff" castShadow shadow-mapSize-width={1024} shadow-mapSize-height={1024} />
        <Seabed frame={f} />
      </Stage>
      <AbsoluteFill style={{background: 'linear-gradient(180deg, rgba(120,220,255,0.18) 0%, rgba(0,0,0,0) 40%)'}} />
      <Stat value="500+" label="submarine cable systems, active and planned" at={p4[1] - 10} until={p4[2] + 30} x={110} y={140} size={130} />
      <CableSection at={p4[3] - 6} />
      <ShotIn />
      <Sfx name="whoosh" at={0} volume={0.3} />
      <Sfx name="pop" at={p4[3] - 6} volume={0.35} />
    </AbsoluteFill>
  );
};

const ShotThought: React.FC<{L: ReturnType<typeof useLines>; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const p5 = L.n5.p.map((x) => x - base);
  const at = p5[0] + 8;
  const s = pop(f, fps, at, 10);
  return (
    <AbsoluteFill>
      <Stage bg="#040714" cam={[{f: 0, pos: [0, 0.8, 5.8], target: [0, 0, 0], fov: 38}, {f: dur, pos: [0, 1.6, 11], target: [0, 0, 0], fov: 38}]}>
        <ambientLight intensity={0.25} />
        <directionalLight position={[-6, 3, 6]} intensity={1.6} color="#bcd7ff" />
        <Stars count={1500} />
        <Globe frame={f + 2000} rotation={-0.9 - f * 0.003} connectivity={1} />
      </Stage>
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>
        <div
          style={{
            transform: `rotate(-5deg) scale(${1.5 - 0.5 * Math.min(1, s)})`,
            opacity: Math.min(1, s),
            border: `8px solid ${C.amber}`,
            borderRadius: 16,
            padding: '16px 46px',
            textAlign: 'center',
            background: 'rgba(4,7,20,0.6)',
          }}
        >
          <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 104, color: C.amber, letterSpacing: 6}}>THOUGHT EXPERIMENT</div>
          <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 34, color: C.white, opacity: prog(f, p5[1], p5[1] + 15)}}>grounded in real outages</div>
        </div>
      </AbsoluteFill>
      <ShotIn />
      <Sfx name="hit" at={at} volume={0.4} />
    </AbsoluteFill>
  );
};

export const Network: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.n2.from - 10, b = L.n4.from - 6, c = L.n5.from - 6;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="switch">
        <ShotSwitch stampAt={L.n1.p[1] + 26} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="graph">
        <ShotGraph L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="seabed">
        <ShotSeabed L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={s.duration - c} name="thought">
        <ShotThought L={L} base={c} dur={s.duration - c} />
      </Sequence>
    </AbsoluteFill>
  );
};
