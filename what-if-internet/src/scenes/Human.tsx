import React, {useMemo} from 'react';
import * as THREE from 'three';
import {AbsoluteFill, Sequence, useCurrentFrame, useVideoConfig} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {Globe, PLACES, Stars} from '../components/three/Globe';
import {Island, Ocean} from '../components/three/Props';
import {Street} from '../components/three/City';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {useLines, useScene} from '../lib/timeline';
import {Headline, IconCard, ShotIn, Stat, Tag} from '../components/ui/Overlays';
import {easeInOut, pop, prog} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const EventCard: React.FC<{year: string; place: string; detail: string; at: number; color?: string}> = ({year, place, detail, at, color = C.alert}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  return (
    <div style={{position: 'absolute', left: 110, top: 300, opacity: Math.min(1, s), transform: `translateX(${(1 - s) * -60}px)`, padding: '30px 40px', borderRadius: 26, background: 'rgba(7,11,30,0.8)', borderLeft: `8px solid ${color}`, width: 620}}>
      <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 110, color, lineHeight: 1}}>{year}</div>
      <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 56, color: C.white, marginTop: 8}}>{place}</div>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 34, color: C.mist, marginTop: 10}}>{detail}</div>
    </div>
  );
};

/* h1–h2: globe turns to Egypt */
const ShotEgypt: React.FC<{L: LinesT; dur: number}> = ({L, dur}) => {
  const f = useCurrentFrame();
  const egyptRot = ((-90 - PLACES.cairo[1]) * Math.PI) / 180;
  const rot = -0.6 + (egyptRot + 2 * Math.PI * 0 - -0.6) * easeInOut(prog(f, 0, L.h2.from + 20, (x) => x));
  const mk = prog(f, L.h2.from, L.h2.from + 15);
  return (
    <AbsoluteFill>
      <Stage bg="#040714" cam={[{f: 0, pos: [0, 1, 9], target: [0, 0, 0], fov: 36}, {f: L.h2.from + 20, pos: [-1.2, 1.2, 6.2], target: [-1.6, 0.4, 0], fov: 36}, {f: dur, pos: [-1.0, 1.1, 5.8], target: [-1.6, 0.4, 0], fov: 36}]}>
        <ambientLight intensity={0.3} />
        <directionalLight position={[-6, 3, 6]} intensity={1.5} color="#bcd7ff" />
        <Stars />
        <Globe frame={f} rotation={rot} tilt={0.45} connectivity={1} markers={mk > 0 ? [{lat: PLACES.cairo[0], lon: PLACES.cairo[1], color: C.alert, strength: mk, size: 1.6}] : []} />
      </Stage>
      <Headline text="Shutdowns happen in real life" at={10} until={L.h2.from} y={110} size={84} center />
      <EventCard year="2011" place="Egypt" detail="Cut off from the internet during mass protests — about five days" at={L.h2.from + 10} />
      <Sfx name="pulse" at={L.h2.from + 10} volume={0.5} />
    </AbsoluteFill>
  );
};

/* h3: Tonga eruption severs the cable */
const ShotTonga: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.h3.p.map((x) => x - base);
  const erupt = prog(f, p[0] - 10, p[0] + 20);
  const snap = p[0] + 40;
  const cable = useMemo(() => {
    const pts = [new THREE.Vector3(5, -0.4, 4), new THREE.Vector3(20, -0.45, 10), new THREE.Vector3(40, -0.45, 12), new THREE.Vector3(70, -0.45, 14)];
    return new THREE.CatmullRomCurve3(pts);
  }, []);
  const geoA = useMemo(() => new THREE.TubeGeometry(cable, 80, 0.18, 8), [cable]);
  const broken = f > snap;
  const col = broken ? C.alert : C.data;
  return (
    <AbsoluteFill>
      <Stage bg="#7fb6d8" fog={['#7fb6d8', 40, 130]} shadows cam={[{f: 0, pos: [-24, 14, 34], target: [4, 4, 0], fov: 40}, {f: dur, pos: [-14, 9, 30], target: [8, 3, 4], fov: 40}]}>
        <Lights intensity={0.9} shadow shadowSize={30} sunPos={[20, 30, 10]} />
        <Ocean color="#2d7fb3" y={-0.2} />
        <Island position={[0, 0, 0]} erupt={erupt} frame={f} />
        <mesh geometry={geoA}>
          <meshBasicMaterial color={col} toneMapped={false} transparent opacity={broken ? 0.9 : 1} />
        </mesh>
        {broken ? (
          <mesh position={cable.getPointAt(0.3)} scale={1 + Math.sin(f * 0.4) * 0.15}>
            <sphereGeometry args={[0.9, 16, 12]} />
            <meshBasicMaterial color={C.alert} toneMapped={false} transparent opacity={0.6} />
          </mesh>
        ) : null}
      </Stage>
      <AbsoluteFill style={{background: '#5b4a52', mixBlendMode: 'multiply', opacity: 0.5 * erupt}} />
      <Tag text="Only international cable: severed" icon="cable" x={1000} y={150} at={snap} color={C.alert} />
      <EventCard year="2022" place="Tonga" detail="Volcanic eruption cut its only international cable — about five weeks to fully reconnect" at={p[1] - 10} color={C.amber} />
      <ShotIn />
      <Sfx name="boom" at={p[0] - 10} volume={0.8} />
      <Sfx name="riser" at={p[0] - 40} volume={0.25} />
      <Sfx name="glitch" at={snap} volume={0.4} />
    </AbsoluteFill>
  );
};

/* h4: Access Now count */
const ShotCount: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.h4.p.map((x) => x - base);
  const filled = Math.floor(296 * prog(f, p[0] - 20, p[1] + 10, (x) => x));
  return (
    <AbsoluteFill style={{background: 'radial-gradient(ellipse at 60% 50%, #18224d, #070b1e 70%)'}}>
      <div style={{position: 'absolute', right: 120, top: 190, display: 'grid', gridTemplateColumns: 'repeat(24, 26px)', gap: 7}}>
        {Array.from({length: 312}).map((_, i) => (
          <div key={i} style={{width: 26, height: 26, borderRadius: 6, background: i < filled ? C.alert : i < 296 ? 'rgba(255,255,255,0.08)' : 'transparent', boxShadow: i < filled && i > filled - 12 ? `0 0 14px ${C.alert}` : undefined}} />
        ))}
      </div>
      <Stat value={String(Math.max(0, filled))} label="government-ordered internet shutdowns documented in 2024" at={p[0] - 20} x={110} y={300} size={170} color={C.alert} />
      <div style={{position: 'absolute', left: 110, top: 620, fontFamily: FONT_BODY, fontWeight: 600, fontSize: 36, color: C.mist, opacity: prog(f, p[1], p[1] + 15)}}>across 54 countries · Access Now #KeepItOn</div>
      <ShotIn />
      {Array.from({length: 10}).map((_, i) => (
        <Sfx key={i} name="tick" at={p[0] - 20 + i * 15} volume={0.2} />
      ))}
    </AbsoluteFill>
  );
};

/* h5: costs vs adapting */
const ShotNeighbours: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.h5.p.map((x) => x - base);
  const adapt = f > p[3] - 10;
  const groups: [number, number, number][] = [
    [-4, 4.4, 0.6],
    [-3, 4.9, -2.4],
    [2.5, 4.3, 0.9],
    [3.6, 4.8, -2.1],
    [8, 4.5, 1.2],
  ];
  return (
    <AbsoluteFill>
      <Stage bg={adapt ? '#ffc79a' : '#9fb4cf'} fog={[adapt ? '#ffc79a' : '#9fb4cf', 30, 90]} shadows cam={[{f: 0, pos: [-2, 4, 17], target: [1, 1.5, 2], fov: 38}, {f: dur, pos: [3, 3, 13], target: [2, 1.5, 3], fov: 38}]}>
        <Lights intensity={0.95} sun={adapt ? '#ffc59a' : '#dfe7ff'} shadow shadowSize={20} sunPos={adapt ? [-14, 10, 12] : [8, 18, 10]} />
        <Street frame={f * 0.3} seed="village" nearSide={false} length={60} />
        {/* shop stall */}
        <group position={[6.5, 0, 4.2]}>
          <mesh position={[0, 0.5, 0]} castShadow>
            <boxGeometry args={[2.2, 1, 0.8]} />
            <meshStandardMaterial color="#a0754d" />
          </mesh>
          <mesh position={[0, 2.2, 0]}>
            <boxGeometry args={[2.6, 0.1, 1.4]} />
            <meshStandardMaterial color="#ff6b6b" />
          </mesh>
          <mesh position={[0.3, 1.02, 0.1]} rotation={[-Math.PI / 2, 0, 0.2]}>
            <planeGeometry args={[0.5, 0.36]} />
            <meshStandardMaterial color="#f4f1e6" />
          </mesh>
        </group>
        {groups.map(([x, z, r], i) => (
          <Person
            key={i}
            look={lookFor('nb' + i)}
            position={[x, 0.06, z]}
            rotation={r}
            pose={{
              mood: adapt ? 0.8 : -0.5,
              blink: isBlink(f, 'nb' + i),
              armR: adapt && i % 2 === 0 ? -0.6 + Math.sin(f * 0.15 + i) * 0.4 : 0,
              headTilt: Math.sin(f * 0.06 + i) * 0.1,
              hold: i === 4 && adapt ? 'clipboard' : 'none',
              armL: i === 4 && adapt ? -0.6 : 0,
              shrug: adapt ? 0 : 0.4,
            }}
          />
        ))}
      </Stage>
      <div style={{position: 'absolute', left: 0, right: 0, top: 110, display: 'flex', justifyContent: 'center', gap: 26, opacity: 1 - prog(f, p[3] - 10, p[3] + 4)}}>
        <IconCard icon="cash" label="Lost income" state="down" at={p[0] - 6} stateAt={p[0] + 6} width={240} />
        <IconCard icon="family" label="Families cut off" state="down" at={p[1]} stateAt={p[1] + 12} width={240} />
        <IconCard icon="building" label="Businesses frozen" state="down" at={p[2]} stateAt={p[2] + 12} width={240} />
      </div>
      <div style={{position: 'absolute', left: 0, right: 0, top: 110, display: 'flex', justifyContent: 'center', gap: 26}}>
        {f > p[3] ? (
          <>
            <IconCard icon="comment" label="Neighbors share news" state="ok" at={p[4]} stateAt={p[4] + 12} width={270} />
            <IconCard icon="notebook" label="Debts in notebooks" state="ok" at={p[5]} stateAt={p[5] + 12} width={270} />
            <IconCard icon="users" label="Life gets more local" state="ok" at={p[6]} stateAt={p[6] + 12} width={270} />
          </>
        ) : null}
      </div>
      <Tag text="But people adapt" icon="users" x={960} y={950} at={p[3]} color={C.ok} size={36} />
      <ShotIn />
      <Sfx name="city" at={0} volume={0.25} />
      <Sfx name="chime" at={p[3]} volume={0.3} />
    </AbsoluteFill>
  );
};

export const Human: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.h3.from - 8, b = L.h4.from - 8, c = L.h5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="egypt">
        <ShotEgypt L={L} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="tonga">
        <ShotTonga L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="count">
        <ShotCount L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={s.duration - c} name="neighbours">
        <ShotNeighbours L={L} base={c} dur={s.duration - c} />
      </Sequence>
    </AbsoluteFill>
  );
};
