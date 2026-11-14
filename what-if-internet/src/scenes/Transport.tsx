import React, {useMemo} from 'react';
import {AbsoluteFill, Sequence, useCurrentFrame} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {ControlTower, Crane, Desk, Ocean, Plane, Ship, Truck, Container} from '../components/three/Props';
import {Building} from '../components/three/City';
import {useLines, useScene} from '../lib/timeline';
import {CardRow, Headline, IconCard, ShotIn, Stat, Tag} from '../components/ui/Overlays';
import {DepartureBoard, Flight} from '../components/ui/DepartureBoard';
import {clamp, prog, rnd} from '../lib/anim';
import {C} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const Clouds: React.FC<{y?: number; count?: number; spread?: number}> = ({y = 0, count = 26, spread = 80}) => {
  const items = useMemo(
    () => Array.from({length: count}).map((_, i) => ({x: (rnd('cx' + i) - 0.5) * spread, z: (rnd('cz' + i) - 0.5) * spread * 0.6, s: 1.5 + rnd('cs' + i) * 2.5})),
    [count, spread],
  );
  return (
    <group position={[0, y, 0]}>
      {items.map((c, i) => (
        <group key={i} position={[c.x, 0, c.z]} scale={c.s}>
          {[0, 1, 2].map((k) => (
            <mesh key={k} position={[k * 0.9 - 0.9, k === 1 ? 0.35 : 0, 0]}>
              <icosahedronGeometry args={[k === 1 ? 1 : 0.75, 1]} />
              <meshStandardMaterial color="#ffffff" emissive="#dfe8ff" emissiveIntensity={0.45} flatShading roughness={1} />
            </mesh>
          ))}
        </group>
      ))}
    </group>
  );
};

const Runway: React.FC = () => (
  <>
    <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
      <planeGeometry args={[400, 400]} />
      <meshStandardMaterial color="#8fb08a" />
    </mesh>
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 6]} receiveShadow>
      <planeGeometry args={[200, 7]} />
      <meshStandardMaterial color="#3a3f55" />
    </mesh>
    {Array.from({length: 30}).map((_, i) => (
      <mesh key={i} rotation={[-Math.PI / 2, 0, 0]} position={[-90 + i * 6, 0.03, 6]}>
        <planeGeometry args={[3, 0.25]} />
        <meshBasicMaterial color="#f4f1e0" />
      </mesh>
    ))}
  </>
);

/* t1: planes fly, radio + radar keep working */
const ShotSky: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.t1.p.map((x) => x - base);
  const px = -40 + f * 0.22;
  return (
    <AbsoluteFill>
      <Stage
        bg="#8fcbff"
        fog={['#8fcbff', 40, 160]}
        shadows
        cam={[
          {f: 0, pos: [px + 10, 26, 18], target: [px, 22, 0], fov: 40},
          {f: p[0] - 10, pos: [px + 6, 22, 16], target: [px, 20, 0], fov: 40},
          {f: p[0] + 25, pos: [-12, 4, 30], target: [0, 6, 0], fov: 46},
          {f: dur, pos: [-8, 3.5, 26], target: [0, 7, 0], fov: 46},
        ]}
      >
        <Lights intensity={1.1} shadow shadowSize={40} sunPos={[20, 40, 20]} />
        <Runway />
        <ControlTower position={[0, 0, -4]} frame={f} />
        {Array.from({length: 4}).map((_, i) => (
          <Building key={i} w={8} d={5} h={4} color="#dfe5f2" position={[-16 + i * 9, 0, -14]} />
        ))}
        <Clouds y={18} />
        <Plane position={[px, 20 - Math.max(0, f - p[0]) * 0.04, 0]} rotation={[0, 0, 0.02]} scale={1.6} />
        {/* radio rings tower <-> plane */}
        {[0, 1, 2].map((k) => {
          const t = (f * 0.015 + k / 3) % 1;
          const vis = prog(f, p[0], p[0] + 15);
          return (
            <mesh key={k} position={[0, 7.4, -4]} scale={1 + t * 16}>
              <sphereGeometry args={[1, 24, 12]} />
              <meshBasicMaterial color={C.ok} wireframe transparent opacity={(1 - t) * 0.35 * vis} toneMapped={false} />
            </mesh>
          );
        })}
      </Stage>
      <Tag text="Flying doesn't need the internet" icon="plane" x={960} y={150} at={20} until={p[0] + 10} color={C.ok} />
      <Tag text="Radio ✓" icon="radio" x={620} y={150} at={p[0] + 15} color={C.ok} />
      <Tag text="Radar ✓" icon="satellite" x={1200} y={150} at={p[1]} color={C.ok} />
      <Sfx name="jet" at={0} volume={0.45} />
      <Sfx name="static" at={p[0] + 15} volume={0.3} />
      <Sfx name="radar" at={p[1]} volume={0.4} />
      <Sfx name="radar" at={p[1] + 45} volume={0.3} />
    </AbsoluteFill>
  );
};

/* t2: airlines run on networks */
const ShotAirline: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.t2.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage bg="#9fd3ff" fog={['#9fd3ff', 30, 120]} shadows cam={[{f: 0, pos: [14, 5, 16], target: [0, 2, 0], fov: 38}, {f: dur, pos: [9, 4, 13], target: [0, 2, 0], fov: 38}]}>
        <Lights intensity={1.05} shadow shadowSize={20} sunPos={[10, 18, 10]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <planeGeometry args={[200, 200]} />
          <meshStandardMaterial color="#59607a" />
        </mesh>
        <Building w={30} d={6} h={6} color="#e6ebf5" position={[0, 0, -9]} />
        <Plane position={[0, 1.6, 0]} rotation={[0, -0.3, 0]} scale={1.7} />
        <Truck position={[6, 0, 3]} rotation={2.6} color="#ffd166" box="#e8ecf7" />
      </Stage>
      <Headline text="Airlines run on networks" at={6} y={110} size={84} />
      <CardRow y={640} gap={30}>
        <IconCard icon="cart" label="Bookings" state="down" at={p[0]} stateAt={p[3] + 40} />
        <IconCard icon="users" label="Check-in" state="down" at={p[1]} stateAt={p[3] + 46} />
        <IconCard icon="clock" label="Crew schedules" state="down" at={p[2]} stateAt={p[3] + 52} />
        <IconCard icon="chart" label="Load planning" state="down" at={p[3]} stateAt={p[3] + 58} />
      </CardRow>
      <ShotIn />
      {[0, 1, 2, 3].map((k) => (
        <Sfx key={k} name="pop" at={p[k]} volume={0.3} />
      ))}
      <Sfx name="error" at={p[3] + 40} volume={0.3} />
    </AbsoluteFill>
  );
};

/* t3: CrowdStrike, July 2024 — a wall of crashed screens */
const ShotCrash: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.t3.p.map((x) => x - base);
  const wave = p[0] + 10;
  const desks = useMemo(() => Array.from({length: 48}).map((_, i) => ({x: (i % 8) * 2.2 - 7.7, z: -Math.floor(i / 8) * 2.4, d: Math.hypot((i % 8) - 3.5, Math.floor(i / 8)) * 6 + rnd('cd' + i) * 8})), []);
  return (
    <AbsoluteFill>
      <Stage bg="#0e1330" fog={['#0e1330', 8, 30]} cam={[{f: 0, pos: [0, 3.4, 7], target: [0, 1, -4], fov: 46}, {f: dur, pos: [0, 6, 9], target: [0, 0.5, -6], fov: 46}]}>
        <ambientLight intensity={0.35} />
        <pointLight position={[0, 6, 2]} intensity={40} color="#cfe0ff" />
        <mesh rotation={[-Math.PI / 2, 0, 0]}>
          <planeGeometry args={[60, 60]} />
          <meshStandardMaterial color="#232a4a" />
        </mesh>
        {desks.map((d, i) => {
          const crashed = f > wave + d.d;
          return <Desk key={i} position={[d.x, 0, d.z]} screen={crashed ? '#1d5fd6' : ['#dfe9ff', '#c9f2e6', '#fff1d6'][i % 3]} />;
        })}
      </Stage>
      <AbsoluteFill style={{background: '#1d5fd6', mixBlendMode: 'soft-light', opacity: 0.5 * prog(f, wave, wave + 80)}} />
      <Headline text="July 2024" sub="A faulty security-software update (CrowdStrike)" at={4} until={p[1]} y={110} size={84} />
      <Stat value="≈ 8.5 million" label="Windows devices crashed" at={p[1] + 4} until={p[3] + 4} x={110} y={110} size={120} color="#7fb2ff" />
      <Stat value="Thousands" label="of flights canceled" at={p[3] + 6} until={p[4]} x={110} y={110} size={120} color={C.alert} icon="plane" />
      <Tag text="…and that wasn't even an internet outage" icon="alert" x={960} y={940} at={p[4] + 4} color={C.amber} size={34} />
      <ShotIn />
      <Sfx name="glitch" at={wave} volume={0.35} />
      <Sfx name="error" at={wave + 20} volume={0.25} />
      <Sfx name="error" at={wave + 45} volume={0.25} />
      <Sfx name="hit" at={p[1] + 4} volume={0.3} />
    </AbsoluteFill>
  );
};

/* t4: board turns red; planes in the air still land */
const FLIGHTS: Flight[] = [
  {time: '07:10', dest: 'Lisbon', code: 'WR 214', status: 'ON TIME'},
  {time: '07:25', dest: 'Nairobi', code: 'WR 880', status: 'ON TIME'},
  {time: '07:40', dest: 'Singapore', code: 'WR 031', status: 'ON TIME'},
  {time: '07:55', dest: 'Toronto', code: 'WR 552', status: 'ON TIME'},
  {time: '08:05', dest: 'São Paulo', code: 'WR 707', status: 'ON TIME'},
  {time: '08:20', dest: 'Seoul', code: 'WR 118', status: 'ON TIME'},
];

const ShotBoard: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.t4.p.map((x) => x - base);
  const land = clamp((f - p[0] + 40) / 120);
  return (
    <AbsoluteFill>
      <Stage bg="#ffc89a" fog={['#ffc89a', 40, 160]} shadows cam={[{f: 0, pos: [30, 6, 34], target: [8, 4, 0], fov: 40}, {f: dur, pos: [26, 4, 30], target: [4, 2, 6], fov: 40}]}>
        <Lights intensity={0.95} sun="#ffd2a6" shadow shadowSize={40} sunPos={[-20, 24, 20]} />
        <Runway />
        <Plane position={[40 - land * 50, 0.9 + Math.max(0, 1 - land * 1.2) * 14, 6]} rotation={[0, Math.PI, 0.06 * (1 - land)]} scale={1.6} livery={C.amber} />
      </Stage>
      <DepartureBoard flights={FLIGHTS} after={['CANCELED', 'DELAYED', 'CANCELED', 'CANCELED', 'DELAYED', 'CANCELED']} flipAt={14} x={90} y={130} opacity={1 - prog(f, p[0] - 10, p[0] + 10) * 0.25} />
      <Tag text="Planes already in the air land safely" icon="check" x={1400} y={900} at={p[0]} color={C.ok} size={32} />
      <ShotIn />
      {Array.from({length: 6}).map((_, i) => (
        <Sfx key={i} name="tick" at={14 + i * 7} volume={0.4} />
      ))}
      <Sfx name="jet" at={p[0] - 40} volume={0.35} />
    </AbsoluteFill>
  );
};

/* t5: ports — cargo fine, paperwork jammed */
const CCOL = ['#e4572e', '#29335c', '#f3a712', '#669bbc', '#a8c686', '#db3a34'];
const ShotPort: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.t5.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage
        bg="#a9d8ff"
        fog={['#a9d8ff', 50, 160]}
        shadows
        cam={[
          {f: 0, pos: [-34, 22, 34], target: [0, 2, 0], fov: 40},
          {f: p[3], pos: [-10, 14, 34], target: [8, 1, 6], fov: 40},
          {f: dur, pos: [6, 9, 30], target: [14, 1, 10], fov: 40},
        ]}
      >
        <Lights intensity={1.05} shadow shadowSize={50} sunPos={[20, 40, 20]} />
        <Ocean y={-0.6} />
        <mesh position={[0, -0.1, 12]} receiveShadow>
          <boxGeometry args={[80, 1, 22]} />
          <meshStandardMaterial color="#b7bcc9" />
        </mesh>
        <Ship position={[-4, -0.2, -2]} seed="port" />
        {[-10, -2, 6].map((x, i) => (
          <Crane key={x} position={[x, 0.4, 3.5]} trolley={0.3 + i * 0.15} />
        ))}
        {Array.from({length: 24}).map((_, i) => (
          <Container key={i} position={[-12 + (i % 6) * 2.7, 0.9 + Math.floor(i / 12) * 1.0, 10 + (Math.floor(i / 6) % 2) * 1.2]} color={CCOL[i % 6]} />
        ))}
        {/* gate + queue of trucks */}
        <mesh position={[14, 1.6, 14]}>
          <boxGeometry args={[0.3, 3.2, 0.3]} />
          <meshStandardMaterial color="#3b4157" />
        </mesh>
        <mesh position={[14, 1.2, 15.8]} rotation={[0, 0, 0]}>
          <boxGeometry args={[0.15, 0.15, 3.6]} />
          <meshStandardMaterial color={C.alert} />
        </mesh>
        {Array.from({length: 6}).map((_, i) => (
          <Truck key={i} position={[17 + i * 5.2, 0.4, 16]} rotation={Math.PI} color={['#e4572e', '#2ec4b6', '#ffd166', '#6c8cff', '#f78fb3', '#9b8cff'][i]} box={CCOL[(i + 2) % 6]} />
        ))}
      </Stage>
      <Tag text="Cargo: fine" icon="check" x={480} y={150} at={p[0]} color={C.ok} />
      <Tag text="Paperwork: jammed" icon="doc" x={1000} y={150} at={p[1] + 4} color={C.alert} />
      <CardRow y={680} gap={30}>
        <IconCard icon="ship" label="Containers" state="warn" at={p[2]} stateAt={p[3] + 30} width={230} />
        <IconCard icon="shield" label="Customs" state="warn" at={p[3]} stateAt={p[3] + 36} width={230} />
        <IconCard icon="truck" label="Trucks" state="warn" at={p[3] + 18} stateAt={p[3] + 42} width={230} />
      </CardRow>
      <Tag text="Shipments wait at the gates" icon="hourglass" x={1400} y={950} at={p[4]} until={p[5] + 10} color={C.amber} />
      <Tag text="Just-in-time supply chains stall" icon="alert" x={1400} y={950} at={p[5] + 10} color={C.alert} />
      <ShotIn />
      <Sfx name="city" at={0} volume={0.3} />
      <Sfx name="chime" at={p[0]} volume={0.25} />
      <Sfx name="error" at={p[1] + 4} volume={0.3} />
      <Sfx name="pop" at={p[2]} volume={0.25} />
      <Sfx name="pop" at={p[3]} volume={0.25} />
      <Sfx name="pop" at={p[3] + 18} volume={0.25} />
    </AbsoluteFill>
  );
};

export const Transport: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.t2.from - 8, b = L.t3.from - 8, c = L.t4.from - 8, d = L.t5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="sky">
        <ShotSky L={L} base={0} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="airline">
        <ShotAirline L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="crash">
        <ShotCrash L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="board">
        <ShotBoard L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="port">
        <ShotPort L={L} base={d} dur={s.duration - d} />
      </Sequence>
    </AbsoluteFill>
  );
};
