import React from 'react';
import {AbsoluteFill, Sequence, useCurrentFrame} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {Skyline} from '../components/three/City';
import {Box, Desk, RobotArm} from '../components/three/Props';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {useLines, useScene} from '../lib/timeline';
import {CardRow, Headline, IconCard, ShotIn, Tag} from '../components/ui/Overlays';
import {prog} from '../lib/anim';
import {C} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const ShotEurope: React.FC<{L: LinesT; dur: number}> = ({L, dur}) => {
  const f = useCurrentFrame();
  const p = L.w1.p;
  const sun = prog(f, 0, dur);
  return (
    <AbsoluteFill>
      <Stage bg="#ffd0a8" fog={['#ffd0a8', 50, 150]} shadows cam={[{f: 0, pos: [-40, 18, 50], target: [0, 4, 0], fov: 36}, {f: dur, pos: [-30, 14, 42], target: [0, 5, 0], fov: 36}]}>
        <Lights intensity={0.7 + sun * 0.4} sun="#ffc08a" sky="#ffd9b8" shadow shadowSize={50} sunPos={[-40, 8 + sun * 14, -10]} />
        <Skyline size={5} seed="europe" frame={f} night={0.5 - sun * 0.5} ground="#94ae8c" />
        <mesh position={[-90, 10 + sun * 12, -80]}>
          <sphereGeometry args={[6, 24, 16]} />
          <meshBasicMaterial color="#ffe2a6" toneMapped={false} />
        </mesh>
      </Stage>
      <Tag text="09:00 UTC · Europe's working day begins" icon="clock" x={960} y={150} at={L.w1.from + p[0]} until={L.w1.from + p[2]} color={C.amber} size={34} />
      <Headline text="Modern work lives in the cloud" at={L.w1.from + p[2]} y={110} size={84} center />
      <Sfx name="whooshShort" at={L.w1.from + p[2]} volume={0.3} />
    </AbsoluteFill>
  );
};

const ShotOffice: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.w2.p.map((x) => x - base);
  const leave = p[4] + 10;
  const down = (i: number) => f > p[Math.min(4, i)] + 20;
  return (
    <AbsoluteFill>
      <Stage bg="#e9eef8" shadows cam={[{f: 0, pos: [8, 7, 11], target: [0, 1, -1], fov: 40}, {f: dur, pos: [5, 5.5, 9], target: [0, 1, -1], fov: 40}]}>
        <Lights intensity={1} shadow shadowSize={12} sunPos={[6, 12, 8]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <planeGeometry args={[40, 40]} />
          <meshStandardMaterial color="#c9ced9" />
        </mesh>
        <mesh position={[0, 3, -5]} receiveShadow>
          <planeGeometry args={[40, 6]} />
          <meshStandardMaterial color="#dfe4ee" />
        </mesh>
        {Array.from({length: 6}).map((_, i) => {
          const x = (i % 3) * 3 - 3;
          const z = Math.floor(i / 3) * 3 - 2;
          const look = lookFor('office' + i);
          const walkOut = Math.max(0, f - leave - i * 6);
          return (
            <group key={i}>
              <Desk position={[x, 0, z]} screen={down(i) ? '#8a93ad' : ['#cfe3ff', '#d6f5e3', '#fff1d6'][i % 3]} />
              <Person
                look={look}
                position={[x + walkOut * 0.06, 0, z + 0.85]}
                rotation={walkOut > 0 ? Math.PI / 2 : Math.PI}
                pose={{walk: walkOut * 0.3, walkAmp: walkOut > 0 ? 0.7 : 0, mood: down(i) ? -0.5 : 0.5, blink: isBlink(f, 'o' + i), shrug: down(i) && walkOut === 0 ? 0.6 : 0}}
              />
            </group>
          );
        })}
      </Stage>
      <CardRow y={130} gap={22}>
        <IconCard icon="mail" label="Email" state="down" at={0} stateAt={p[0] + 4} width={200} />
        <IconCard icon="doc" label="Documents" state="down" at={p[0]} stateAt={p[1] + 4} width={200} />
        <IconCard icon="video" label="Video calls" state="down" at={p[1]} stateAt={p[2] + 4} width={200} />
        <IconCard icon="payroll" label="Payroll" state="down" at={p[2]} stateAt={p[3] + 4} width={200} />
        <IconCard icon="cart" label="Stock & orders" state="down" at={p[3]} stateAt={p[4] - 10} width={200} />
      </CardRow>
      <Tag text="Staff may be sent home" icon="users" x={960} y={960} at={leave} color={C.amber} size={34} />
      <ShotIn />
      <Sfx name="typing" at={0} volume={0.3} />
      {[0, 1, 2, 3].map((k) => (
        <Sfx key={k} name="error" at={p[k] + 4} volume={0.2} />
      ))}
    </AbsoluteFill>
  );
};

const ShotFactory: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.w3.p.map((x) => x - base);
  const boxes = Array.from({length: 14});
  return (
    <AbsoluteFill>
      <Stage bg="#2a3152" fog={['#2a3152', 15, 45]} shadows cam={[{f: 0, pos: [-9, 6, 12], target: [0, 1, 0], fov: 40}, {f: dur, pos: [6, 5, 11], target: [1, 1, 0], fov: 40}]}>
        <Lights intensity={0.9} sky="#cfd8ff" shadow shadowSize={14} sunPos={[4, 12, 6]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <planeGeometry args={[60, 60]} />
          <meshStandardMaterial color="#59607a" />
        </mesh>
        {/* conveyor */}
        <mesh position={[0, 0.8, 0]} castShadow receiveShadow>
          <boxGeometry args={[16, 0.2, 1.4]} />
          <meshStandardMaterial color="#20243a" />
        </mesh>
        {boxes.map((_, i) => {
          const x = ((f * 0.04 + i * 1.3) % 18) - 9;
          const piled = x > 7;
          return <Box key={i} position={[piled ? 7.3 + (i % 3) * 0.55 : x, piled ? 1.12 + (i % 2) * 0.42 : 1.12, piled ? -0.3 + (i % 2) * 0.5 : 0]} color={['#c89b6a', '#d9ad7c'][i % 2]} />;
        })}
        {[-4, 0, 4].map((x, i) => (
          <RobotArm key={x} position={[x, 0, -1.8]} a={Math.sin(f * 0.05 + i) * 0.8} b={-0.4 + Math.sin(f * 0.07 + i) * 0.3} c={-1.2 + Math.cos(f * 0.06 + i) * 0.4} color={['#ffb547', '#4ecdc4', '#ff6b6b'][i]} />
        ))}
      </Stage>
      <Tag text="Local control systems" icon="factory" x={560} y={150} at={10} color={C.ok} />
      <Tag text="Machines keep running" icon="check" x={1120} y={150} at={p[0] + 10} color={C.ok} />
      <CardRow y={720} gap={30}>
        <IconCard icon="cart" label="New orders" state="warn" at={p[1] + 10} stateAt={p[1] + 40} width={230} />
        <IconCard icon="factory" label="New parts" state="warn" at={p[2]} stateAt={p[2] + 26} width={230} />
        <IconCard icon="truck" label="Shipping info" state="warn" at={p[3]} stateAt={p[3] + 26} width={230} />
      </CardRow>
      <ShotIn />
      <Sfx name="hum" at={0} volume={0.25} />
      <Sfx name="pop" at={p[1] + 10} volume={0.25} />
      <Sfx name="pop" at={p[2]} volume={0.25} />
      <Sfx name="pop" at={p[3]} volume={0.25} />
    </AbsoluteFill>
  );
};

export const Work: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.w2.from - 8, b = L.w3.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="europe">
        <ShotEurope L={L} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="office">
        <ShotOffice L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={s.duration - b} name="factory">
        <ShotFactory L={L} base={b} dur={s.duration - b} />
      </Sequence>
    </AbsoluteFill>
  );
};
