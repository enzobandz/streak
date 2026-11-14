import React, {useMemo} from 'react';
import * as THREE from 'three';
import {AbsoluteFill, Sequence, useCurrentFrame} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {BroadcastTower, HospitalBuilding, Pylon, WaterPlant} from '../components/three/Props';
import {Building, Tree} from '../components/three/City';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {useLines, useScene} from '../lib/timeline';
import {Bubble, CardRow, Headline, IconCard, ShotIn, Tag} from '../components/ui/Overlays';
import {prog, rnd} from '../lib/anim';
import {C} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const Ground: React.FC<{color?: string}> = ({color = '#86ad7f'}) => (
  <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
    <planeGeometry args={[500, 500]} />
    <meshStandardMaterial color={color} />
  </mesh>
);

/* Power grid: plant -> pylons -> lit town */
const ShotGrid: React.FC<{L: LinesT; dur: number}> = ({L, dur}) => {
  const f = useCurrentFrame();
  const pylons = [-30, -18, -6, 6, 18];
  const wire = useMemo(() => {
    const pts = pylons.map((x) => new THREE.Vector3(x, 5.9, 0));
    pts.unshift(new THREE.Vector3(-40, 4, 0));
    pts.push(new THREE.Vector3(28, 3, 0));
    return new THREE.CatmullRomCurve3(pts);
  }, []);
  const tube = useMemo(() => new THREE.TubeGeometry(wire, 200, 0.05, 6), [wire]);
  const k2 = L.k2;
  return (
    <AbsoluteFill>
      <Stage bg="#f6b78c" fog={['#f6b78c', 40, 140]} shadows cam={[{f: 0, pos: [-34, 12, 34], target: [-10, 3, 0], fov: 40}, {f: k2.from, pos: [-10, 10, 32], target: [4, 3, 0], fov: 40}, {f: dur, pos: [14, 9, 30], target: [24, 3, 0], fov: 40}]}>
        <Lights intensity={0.8} sun="#ffc79e" sky="#ffd6b5" shadow shadowSize={60} sunPos={[-30, 20, 20]} />
        <Ground />
        {/* power station */}
        {[-46, -40].map((x) => (
          <mesh key={x} position={[x, 5, -6]} castShadow>
            <cylinderGeometry args={[2.2, 3.2, 10, 24, 1, true]} />
            <meshStandardMaterial color="#e3e6ef" side={THREE.DoubleSide} />
          </mesh>
        ))}
        <Building w={8} d={6} h={5} color="#cbd5f0" position={[-43, 0, 3]} roof />
        {pylons.map((x) => (
          <Pylon key={x} position={[x, 0, 0]} />
        ))}
        <mesh geometry={tube}>
          <meshStandardMaterial color="#3b4157" />
        </mesh>
        {Array.from({length: 8}).map((_, i) => {
          const t = (f * 0.006 + i / 8) % 1;
          return (
            <mesh key={i} position={wire.getPointAt(t)} scale={0.35}>
              <sphereGeometry args={[1, 12, 10]} />
              <meshBasicMaterial color={C.ok} toneMapped={false} />
            </mesh>
          );
        })}
        {/* town with lights on */}
        {Array.from({length: 14}).map((_, i) => (
          <Building key={i} w={3 + rnd('tw' + i) * 2} d={3} h={3 + rnd('th' + i) * 6} color={['#f5e1c8', '#d9d2f5', '#bfe3e0'][i % 3]} position={[24 + (i % 7) * 5, 0, -6 + Math.floor(i / 7) * 8]} night={0.9} />
        ))}
        {Array.from({length: 10}).map((_, i) => (
          <Tree key={i} position={[-20 + i * 5, 0, 8 + (i % 3)]} s={1.4} />
        ))}
      </Stage>
      <Headline text="What keeps working?" sub="Many critical systems are built to survive exactly this" at={L.k1.from - 10} until={k2.from} y={110} size={84} />
      <Tag text="Dedicated control networks" icon="network" x={600} y={150} at={k2.from + 30} color={C.ok} />
      <Tag text="Kept apart from the public internet" icon="shield" x={1100} y={230} at={k2.p[0] - 10} color={C.ok} />
      <Tag text="The lights should stay on" icon="bolt" x={960} y={940} at={k2.p[1]} color={C.amber} size={36} />
      <Sfx name="hum" at={k2.from} volume={0.35} />
      <Sfx name="chime" at={k2.p[1]} volume={0.3} />
    </AbsoluteFill>
  );
};

const ShotWater: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.k2b.p.map((x) => x - base);
  const op = lookFor('operator');
  return (
    <AbsoluteFill>
      <Stage bg="#b9e2ff" fog={['#b9e2ff', 30, 100]} shadows cam={[{f: 0, pos: [-10, 9, 14], target: [0, 1, 0], fov: 40}, {f: dur, pos: [-4, 5, 11], target: [1, 1, 0], fov: 40}]}>
        <Lights intensity={1.05} shadow shadowSize={20} sunPos={[10, 18, 10]} />
        <Ground color="#9cbf94" />
        <WaterPlant frame={f} />
        <Building w={4} d={3} h={3} color="#e8ecf7" position={[6.5, 0, 1]} />
        <Person look={op} position={[4.6, 0, 3.4]} rotation={-0.9} pose={{hold: 'clipboard', armL: -0.6, mood: 0.7, blink: isBlink(f, 'op')}} />
      </Stage>
      <Tag text="Water treatment" icon="water" x={480} y={150} at={4} color={C.data} />
      <Tag text="Local control systems" icon="server" x={1000} y={150} at={p[0] + 10} color={C.ok} />
      <Tag text="Operators can manage on site" icon="users" x={960} y={950} at={p[1]} color={C.ok} />
      <ShotIn />
      <Sfx name="pop" at={p[0] + 10} volume={0.3} />
      <Sfx name="pop" at={p[1]} volume={0.3} />
    </AbsoluteFill>
  );
};

const ShotHospital: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.k3.p.map((x) => x - base);
  const nurse = lookFor('nurse3');
  const runner = lookFor('runner');
  const runX = -8 + ((f - p[2]) > 0 ? (f - p[2]) * 0.07 : 0);
  return (
    <AbsoluteFill>
      <Stage bg="#cfe9ff" fog={['#cfe9ff', 40, 120]} shadows cam={[{f: 0, pos: [-6, 6, 22], target: [0, 4, 0], fov: 40}, {f: p[0], pos: [-2, 2.4, 12], target: [0, 1.6, 4], fov: 40}, {f: dur, pos: [2, 2.2, 11], target: [0, 1.5, 4], fov: 40}]}>
        <Lights intensity={1.05} shadow shadowSize={20} sunPos={[10, 18, 12]} />
        <Ground color="#a8c49f" />
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 5]} receiveShadow>
          <planeGeometry args={[40, 6]} />
          <meshStandardMaterial color="#d7dae4" />
        </mesh>
        <HospitalBuilding position={[0, 0, -2]} />
        <Person look={{...nurse, shirt: '#4ecdc4', pants: '#4ecdc4'}} position={[-1.5, 0, 4.5]} rotation={0.3} pose={{hold: 'clipboard', armL: -0.7, mood: 0.4, blink: isBlink(f, 'nu')}} />
        <Person
          look={{...runner, shirt: '#ffffff', pants: '#6c8cff'}}
          position={[runX, 0, 5.6]}
          rotation={Math.PI / 2}
          pose={{walk: f * 0.35, walkAmp: f > p[2] ? 0.9 : 0, hold: 'papers', armR: -0.8, mood: 0.2, blink: isBlink(f, 'ru')}}
        />
      </Stage>
      <Headline text="Downtime procedures" sub="Hospitals plan for computer outages" at={6} until={p[0] + 10} y={110} size={84} />
      <CardRow y={120} gap={30}>
        <IconCard icon="doc" label="Paper charts" state="ok" at={p[0] + 14} stateAt={p[0] + 30} width={260} />
        <IconCard icon="notebook" label="Printed med lists" state="ok" at={p[1] + 6} stateAt={p[1] + 22} width={260} />
        <IconCard icon="users" label="Results by hand" state="ok" at={p[2] + 6} stateAt={p[2] + 22} width={260} />
      </CardRow>
      <Tag text="Slower and riskier — but care continues" icon="hospital" x={960} y={960} at={p[3]} color={C.amber} size={34} />
      <ShotIn />
      <Sfx name="pop" at={p[0] + 14} volume={0.3} />
      <Sfx name="pop" at={p[1] + 6} volume={0.3} />
      <Sfx name="pop" at={p[2] + 6} volume={0.3} />
    </AbsoluteFill>
  );
};

const CALL_BUBBLES = [
  {x: 380, y: 470, t: 'Calling Mom…'},
  {x: 1500, y: 420, t: 'Network busy'},
  {x: 760, y: 360, t: 'Calling…'},
  {x: 1180, y: 560, t: 'Call failed'},
  {x: 260, y: 700, t: 'Redialing…'},
  {x: 1660, y: 700, t: 'Calling Dad…'},
];

const ShotPhones: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.k4.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage bg="#2d2b5a" fog={['#2d2b5a', 30, 90]} cam={[{f: 0, pos: [0, 12, 30], target: [0, 4, 0], fov: 40}, {f: dur, pos: [6, 10, 26], target: [0, 4, 0], fov: 40}]}>
        <Lights intensity={0.55} sky="#8f86e0" ground="#1b1636" />
        <Ground color="#3b4a5a" />
        {Array.from({length: 24}).map((_, i) => (
          <Building key={i} w={3} d={3} h={2 + rnd('ph' + i) * 5} color={['#cdbfe8', '#bfe3e0', '#f5e1c8'][i % 3]} position={[(i % 8) * 5 - 17.5, 0, Math.floor(i / 8) * 6 - 8]} night={1} />
        ))}
        {[-10, 10].map((x, i) => (
          <BroadcastTower key={x} position={[x, 0, -3]} frame={f + i * 20} color={f > p[0] ? C.alert : C.amber} scale={0.9} />
        ))}
      </Stage>
      <Tag text="Emergency services: built with backups" icon="shield" x={960} y={150} at={8} until={p[0] + 6} color={C.ok} />
      <Tag text="But phone networks increasingly run on internet tech" icon="phone" x={960} y={150} at={p[0] + 6} until={p[1] + 10} color={C.amber} size={30} />
      <Tag text="Everyone calls family at once" icon="family" x={960} y={150} at={p[1] + 10} color={C.alert} />
      {CALL_BUBBLES.map((b, i) => (
        <Bubble key={i} x={b.x} y={b.y} icon="phone" color={i % 2 ? C.alert : '#6c8cff'} text={b.t} at={p[1] + 16 + i * 9} />
      ))}
      <ShotIn />
      {CALL_BUBBLES.map((_, i) => (
        <Sfx key={i} name={i % 2 ? 'error' : 'ping'} at={p[1] + 16 + i * 9} volume={0.22} />
      ))}
    </AbsoluteFill>
  );
};

const ShotRadio: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.k5.p.map((x) => x - base);
  const fam = ['fam1', 'fam2', 'fam3'];
  return (
    <AbsoluteFill>
      <Stage bg="#ffd1a3" fog={['#ffd1a3', 30, 110]} shadows cam={[{f: 0, pos: [-14, 6, 18], target: [0, 5, -6], fov: 40}, {f: dur, pos: [-6, 3, 12], target: [0, 2.5, 0], fov: 40}]}>
        <Lights intensity={0.95} sun="#ffd2a6" shadow shadowSize={30} sunPos={[-20, 24, 20]} />
        <Ground color="#9cbf94" />
        <BroadcastTower position={[6, 0, -14]} frame={f} color={C.ok} scale={1.6} />
        {fam.map((sd, i) => (
          <Person key={sd} look={lookFor(sd)} position={[-1.6 + i * 1.3, 0, 2]} rotation={-0.2 + i * 0.15} scale={i === 1 ? 0.75 : 1} pose={{mood: 0.7, blink: isBlink(f, sd), headTilt: Math.sin(f * 0.05 + i) * 0.08}} />
        ))}
        {/* portable radio on a crate */}
        <mesh position={[0, 0.4, 3.6]} castShadow>
          <boxGeometry args={[1, 0.8, 0.8]} />
          <meshStandardMaterial color="#a0754d" />
        </mesh>
        <mesh position={[0, 1.05, 3.6]} castShadow>
          <boxGeometry args={[0.9, 0.5, 0.3]} />
          <meshStandardMaterial color={C.alert} />
        </mesh>
        <mesh position={[0.2, 1.05, 3.76]}>
          <circleGeometry args={[0.15, 16]} />
          <meshStandardMaterial color="#20243a" />
        </mesh>
      </Stage>
      <CardRow y={130} gap={30}>
        <IconCard icon="radio" label="Broadcast radio" state="ok" at={6} stateAt={30} />
        <IconCard icon="tv" label="Broadcast TV" state="ok" at={20} stateAt={44} />
      </CardRow>
      <Tag text="The main source of news for many" icon="users" x={960} y={950} at={p[1]} color={C.amber} size={34} />
      <ShotIn />
      <Sfx name="static" at={4} volume={0.35} />
      <Sfx name="chime" at={30} volume={0.25} />
    </AbsoluteFill>
  );
};

export const Critical: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.k2b.from - 8, b = L.k3.from - 8, c = L.k4.from - 8, d = L.k5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="grid">
        <ShotGrid L={L} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="water">
        <ShotWater L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="hospital">
        <ShotHospital L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="phones">
        <ShotPhones L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="radio">
        <ShotRadio L={L} base={d} dur={s.duration - d} />
      </Sequence>
    </AbsoluteFill>
  );
};
