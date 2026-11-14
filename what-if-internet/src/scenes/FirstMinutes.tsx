import React, {useMemo} from 'react';
import {AbsoluteFill, Sequence, useCurrentFrame, useVideoConfig} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {Skyline, Street, Building} from '../components/three/City';
import {buildGraph, NetworkGraph} from '../components/three/Network';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {Router, Satellite} from '../components/three/Props';
import {Stars} from '../components/three/Globe';
import {useLines, useScene} from '../lib/timeline';
import {Bubble, ShotIn, Tag} from '../components/ui/Overlays';
import {PhoneMap} from '../components/ui/PhoneUI';
import {easeOut, integrate, prog, rnd, windowFade} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const CityLabel: React.FC<{city: string; time: string; note: string; x: number; at: number}> = ({city, time, note, x, at}) => {
  const f = useCurrentFrame();
  const p = prog(f, at, at + 18, easeOut);
  return (
    <div style={{position: 'absolute', left: x, top: 90, opacity: p, transform: `translateY(${(1 - p) * 20}px)`}}>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 30, letterSpacing: 6, color: C.white, textShadow: '0 2px 10px #000'}}>{city}</div>
      <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 96, color: C.white, lineHeight: 1, textShadow: '0 4px 24px rgba(0,0,0,0.6)'}}>{time}</div>
      <div style={{fontFamily: FONT_BODY, fontSize: 28, color: C.white, opacity: 0.85, textShadow: '0 2px 10px #000'}}>{note}</div>
    </div>
  );
};

const ShotCities: React.FC<{dur: number; labelAt: number}> = ({dur, labelAt}) => {
  const f = useCurrentFrame();
  const cam = (z: number) => [
    {f: 0, pos: [44, 40, 60 + z], target: [0, 0, 0], fov: 34},
    {f: dur, pos: [36, 32, 50 + z], target: [0, 2, 0], fov: 34},
  ];
  return (
    <AbsoluteFill>
      <div style={{position: 'absolute', left: 0, top: 0, width: 960, height: 1080, overflow: 'hidden'}}>
        <Stage width={960} height={1080} bg="#a9dcff" fog={['#a9dcff', 60, 140]} shadows cam={cam(0)}>
          <Lights intensity={1.1} shadow shadowSize={50} sunPos={[30, 40, 10]} />
          <Skyline size={5} seed="tokyo" frame={f} night={0} ground="#8db394" />
        </Stage>
      </div>
      <div style={{position: 'absolute', left: 960, top: 0, width: 960, height: 1080, overflow: 'hidden'}}>
        <Stage width={960} height={1080} bg="#3a2c63" fog={['#3a2c63', 50, 130]} cam={cam(4)}>
          <Lights intensity={0.45} sun="#ff9e7a" sky="#7d6bd1" ground="#1b1636" sunPos={[-30, 10, -10]} />
          <Skyline size={5} seed="newyork" frame={f} night={1} ground="#3b4a5a" />
        </Stage>
      </div>
      <div style={{position: 'absolute', left: 957, top: 0, width: 6, height: 1080, background: C.night}} />
      <CityLabel city="TOKYO" time="09:00" note="Morning rush hour" x={80} at={labelAt} />
      <CityLabel city="NEW YORK" time="19:00" note="Early evening (EST)" x={1040} at={labelAt + 14} />
    </AbsoluteFill>
  );
};

const ShotIslands: React.FC<{dur: number; cutAt: number}> = ({dur, cutAt}) => {
  const f = useCurrentFrame();
  const graph = useMemo(() => buildGraph('islands', 8, 9, 7), []);
  const cut = prog(f, cutAt, cutAt + 40);
  return (
    <AbsoluteFill>
      <Stage bg="#080d24" fog={['#080d24', 20, 50]} cam={[{f: 0, pos: [0, 14, 20], target: [0, 0, 0], fov: 40}, {f: dur, pos: [6, 18, 22], target: [0, 0, 0], fov: 42}]}>
        <ambientLight intensity={0.6} />
        <pointLight position={[0, 8, 6]} intensity={40} color="#9fd3ff" />
        <NetworkGraph frame={f} graph={graph} connectivity={1 - cut} islandSpread={cut * 1.3} exchange={1 - cut} />
      </Stage>
      <Tag text="Global routes withdrawn" icon="wifiOff" x={960} y={140} at={cutAt} color={C.alert} />
      <Tag text="Every network becomes an island" icon="network" x={960} y={950} at={cutAt + 50} color={C.amber} />
      <ShotIn />
      <Sfx name="glitch" at={cutAt} volume={0.4} />
      <Sfx name="powerDown" at={cutAt + 4} volume={0.35} />
    </AbsoluteFill>
  );
};

const PEOPLE = [
  {x: -7, z: 4.3, dir: 1, seed: 'a'},
  {x: -2.5, z: 4.9, dir: -1, seed: 'b'},
  {x: 1.5, z: 4.2, dir: 1, seed: 'c'},
  {x: 5.5, z: 4.8, dir: -1, seed: 'd'},
  {x: 9, z: 4.4, dir: 1, seed: 'e'},
  {x: -11, z: 4.7, dir: -1, seed: 'f'},
];

const ShotStreet: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.f2.p.map((x) => x - base);
  const msgs = p[0];
  const freeze = p[1];
  const spin = p[2];
  const slowAt = (fr: number) => 1 - prog(fr, msgs, spin + 30); // people slow down and stop
  const slow = slowAt(f);
  const walked = integrate(f, slowAt);
  const worried = prog(f, msgs + 20, freeze + 20);
  const screenCol = f < freeze ? ['#ff6b6b', '#4ecdc4', '#ffd166', '#6c8cff'][Math.floor(f / 12) % 4] : '#4ecdc4';
  return (
    <AbsoluteFill>
      <Stage
        bg="#9fd3ff"
        fog={['#9fd3ff', 30, 90]}
        shadows
        cam={[
          {f: 0, pos: [0, 2.6, 16], target: [0, 2.2, 0], fov: 36},
          {f: dur, pos: [-1.5, 2.2, 13], target: [0, 1.8, 2], fov: 36},
        ]}
      >
        <Lights intensity={1.05} shadow shadowSize={22} sunPos={[10, 18, 12]} />
        <Street frame={f} traffic={1} seed="main" nearSide={false} />
        {/* big video billboard on the far side */}
        <group position={[2, 5.6, -6.3]}>
          <mesh>
            <boxGeometry args={[6.4, 3.8, 0.2]} />
            <meshStandardMaterial color="#1c2033" />
          </mesh>
          <mesh position={[0, 0, 0.11]}>
            <planeGeometry args={[6, 3.4]} />
            <meshBasicMaterial color={screenCol} toneMapped={false} />
          </mesh>
          <mesh position={[0, 0, 0.12]}>
            <circleGeometry args={[0.9, 3]} />
            <meshBasicMaterial color="#ffffff" toneMapped={false} transparent opacity={f < freeze ? 0 : 0.85} />
          </mesh>
        </group>
        {PEOPLE.map((pp, i) => {
          const walking = (walked + i * 13) * 0.16;
          const x = pp.x + pp.dir * walked * 0.025;
          const look = lookFor('street' + pp.seed);
          return (
            <Person
              key={i}
              look={look}
              position={[x, 0.06, pp.z]}
              rotation={pp.dir > 0 ? Math.PI / 2 - 0.5 : -Math.PI / 2 + 0.5}
              pose={{
                walk: walking,
                walkAmp: 0.6 * slow,
                phone: 1,
                phoneScreen: f > msgs ? '#ffb3b3' : '#7fe7ff',
                mood: 0.7 - worried * 1.7,
                blink: isBlink(f, pp.seed),
                headTilt: worried * Math.sin(i) * 0.2,
              }}
            />
          );
        })}
      </Stage>
      <Bubble x={560} y={560} icon="mail" color={C.alert} text="Not delivered" at={msgs} until={dur} />
      <Bubble x={1340} y={575} icon="comment" color={C.alert} text="Sending…" at={msgs + 14} until={dur} />
      <Bubble x={1180} y={240} icon="spinner" color="#6c8cff" spin at={freeze} until={dur} />
      <Bubble x={860} y={590} icon="spinner" color="#6c8cff" spin at={spin} until={dur} />
      <Bubble x={300} y={600} icon="spinner" color="#6c8cff" spin at={spin + 10} until={dur} />
      <ShotIn />
      <Sfx name="city" at={0} volume={0.35} />
      <Sfx name="ping" at={6} volume={0.3} />
      <Sfx name="error" at={msgs} volume={0.3} />
      <Sfx name="error" at={msgs + 14} volume={0.25} />
      <Sfx name="tick" at={freeze} volume={0.4} />
    </AbsoluteFill>
  );
};

const ShotGPS: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.f3.p.map((x) => x - base);
  const beams = prog(f, p[1], p[1] + 20);
  const look = lookFor('gps');
  return (
    <AbsoluteFill>
      <Stage
        bg="#0d1840"
        cam={[
          {f: 0, pos: [1.5, 1.6, 7], target: [-0.6, 1.5, 0], fov: 40},
          {f: p[1] - 10, pos: [2.5, 0.9, 13], target: [-0.5, 6, -3], fov: 55},
          {f: p[2], pos: [3.5, 0.9, 14], target: [-0.5, 6, -3], fov: 55},
          {f: dur, pos: [3.8, 1.2, 15], target: [-0.5, 5.5, -3], fov: 55},
        ]}
      >
        <Lights intensity={0.7} sky="#9fb8ff" ground="#2a3050" sunPos={[4, 10, 8]} />
        <Stars count={900} />
        <mesh rotation={[-Math.PI / 2, 0, 0]}>
          <circleGeometry args={[30, 48]} />
          <meshStandardMaterial color="#2c3a5e" />
        </mesh>
        <Person look={look} position={[-1, 0, 0]} rotation={0.2} pose={{phone: 1, mood: 0.2, blink: isBlink(f, 'gps'), headPitch: f > p[1] && f < p[2] ? -0.6 : 0}} />
        {[
          [-7, 9, -7],
          [0.5, 11.5, -9],
          [6, 8.5, -5],
        ].map((sp, i) => {
          const pos: [number, number, number] = [sp[0] + Math.sin(f * 0.01 + i) * 0.6, sp[1], sp[2]];
          return (
            <group key={i}>
              <Satellite position={pos} rotation={[0.3, f * 0.004 + i, 0.2]} scale={0.9} />
              {[0, 1, 2].map((k) => {
                const t = (f * 0.02 + k / 3 + i * 0.2) % 1;
                const target = [-0.7, 1.5, 0.2];
                const pt: [number, number, number] = [pos[0] + (target[0] - pos[0]) * t, pos[1] + (target[1] - pos[1]) * t, pos[2] + (target[2] - pos[2]) * t];
                return (
                  <mesh key={k} position={pt} scale={0.12 * beams}>
                    <sphereGeometry args={[1, 10, 8]} />
                    <meshBasicMaterial color={C.ok} toneMapped={false} transparent opacity={beams * (1 - t * 0.5)} />
                  </mesh>
                );
              })}
            </group>
          );
        })}
      </Stage>
      <Tag text="Phone battery: fine" icon="phone" x={420} y={160} at={8} until={p[1]} color={C.ok} />
      <Tag text="GPS still works" icon="satellite" x={420} y={160} at={p[1]} until={dur} color={C.ok} />
      <Tag text="Satellites don't need the internet" icon="check" x={420} y={240} at={p[1] + 40} until={p[2] + 10} color={C.ok} size={26} />
      <PhoneMap x={1320} y={160} tilesLoaded={0} offline={prog(f, p[3], p[3] + 4)} opacity={prog(f, p[2] - 10, p[2] + 10)} />
      <ShotIn />
      <Sfx name="radar" at={p[1]} volume={0.3} />
      <Sfx name="whooshShort" at={p[2] - 10} volume={0.35} />
      <Sfx name="chime" at={p[3]} volume={0.35} />
    </AbsoluteFill>
  );
};

const RouterVisual: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.f4.p.map((x) => x - base);
  const press = p[1] - 30;
  const cutToBlock = p[1] + 4;
  const restarting = f > press;
  const blinkOn = Math.floor(f / 8) % 2 === 0;
  const leds = restarting ? [blinkOn ? C.amber : '#332211', '#331a1a', '#331a1a', '#331a1a'] : [C.ok, C.alert, C.alert, '#331a1a'];
  const look = lookFor('router-guy');
  if (f < cutToBlock) {
    return (
      <AbsoluteFill>
        <Stage bg="#f3e3cf" shadows cam={[{f: 0, pos: [-3.6, 1.9, 3.4], target: [-0.1, 1.1, -0.3], fov: 38}, {f: cutToBlock, pos: [-2.8, 1.6, 2.5], target: [-0.2, 1.05, -0.4], fov: 38}]}>
          <Lights intensity={0.9} sun="#ffe6c4" shadow shadowSize={6} sunPos={[3, 6, 5]} />
          <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
            <planeGeometry args={[20, 20]} />
            <meshStandardMaterial color="#c99f7a" />
          </mesh>
          <mesh position={[0, 3, -1.2]} receiveShadow>
            <planeGeometry args={[20, 6]} />
            <meshStandardMaterial color="#f0dcc4" />
          </mesh>
          <mesh position={[-0.4, 0.9, -0.8]} castShadow receiveShadow>
            <boxGeometry args={[2.2, 0.08, 0.7]} />
            <meshStandardMaterial color="#8a6a4f" />
          </mesh>
          <Router position={[0.05, 0.94, -0.75]} leds={leds} scale={0.55} />
          <mesh position={[0.6, 1.12, -0.85]} castShadow>
            <cylinderGeometry args={[0.15, 0.12, 0.36, 12]} />
            <meshStandardMaterial color="#5fb38a" />
          </mesh>
          <Person
            look={look}
            position={[0.2, 0, 0.35]}
            rotation={-2.9}
            pose={{armR: restarting ? -0.4 : -1.25 * prog(f, 10, press - 5), armRz: -0.2, mood: restarting ? -0.4 : 0.1, blink: isBlink(f, 'rg'), headPitch: 0.25}}
          />
        </Stage>
        <Tag text="Have you tried turning it off and on again?" icon="router" x={960} y={140} at={20} until={cutToBlock} color={C.amber} size={30} />
        <ShotIn />
      </AbsoluteFill>
    );
  }
  const g = f - cutToBlock;
  return (
    <AbsoluteFill>
      <BlockShot g={g} dur={dur} cutToBlock={cutToBlock} />
    </AbsoluteFill>
  );
};

const RouterSfx: React.FC<{press: number; cutToBlock: number}> = ({press, cutToBlock}) => (
  <>
    <Sfx name="switch" at={press} volume={0.5} />
    <Sfx name="blip" at={press + 30} volume={0.3} />
    {Array.from({length: 6}).map((_, k) => (
      <Sfx key={k} name="tick" at={cutToBlock + 6 + k * 5} volume={0.22} />
    ))}
  </>
);

const BlockShot: React.FC<{g: number; dur: number; cutToBlock: number}> = ({g, dur, cutToBlock}) => {
  return (
    <AbsoluteFill>
      <Stage bg="#1b2147" cam={[{f: 0, pos: [0, 6, 22], target: [0, 6, 0], fov: 40}, {f: dur - cutToBlock, pos: [0, 6, 28], target: [0, 6, 0], fov: 40}]}>
        <Lights intensity={0.5} sky="#7d8bd1" />
        <Building w={16} d={4} h={13} color="#cdbfe8" position={[0, 0, -2]} night={0} roof />
        {Array.from({length: 40}).map((_, i) => {
          const cx = (i % 8) - 3.5;
          const cy = Math.floor(i / 8);
          const blinkPhase = Math.floor((g + rnd('rb' + i) * 4) / 8) % 2 === 0;
          return (
            <group key={i} position={[cx * 1.9, 1.6 + cy * 2.4, 0.02]}>
              <mesh>
                <planeGeometry args={[1.4, 1.7]} />
                <meshBasicMaterial color={rnd('wcol' + i) < 0.6 ? '#ffe7b0' : '#cfe8ff'} toneMapped={false} />
              </mesh>
              <mesh position={[0, -0.55, 0.01]}>
                <boxGeometry args={[0.5, 0.12, 0.04]} />
                <meshBasicMaterial color="#f4f5fa" />
              </mesh>
              <mesh position={[0, -0.55, 0.04]}>
                <circleGeometry args={[0.09, 10]} />
                <meshBasicMaterial color={blinkPhase ? C.amber : '#5a3a10'} toneMapped={false} />
              </mesh>
            </group>
          );
        })}
      </Stage>
      <div style={{position: 'absolute', left: 0, right: 0, bottom: 120, textAlign: 'center', opacity: windowFade(g, 30, 9999, 14)}}>
        <div style={{display: 'inline-block', padding: '14px 34px', borderRadius: 40, background: 'rgba(7,11,30,0.8)', fontFamily: FONT_BODY, fontWeight: 600, fontSize: 36, color: C.white}}>
          Millions of restarts… <span style={{color: C.alert}}>it won't help this time.</span>
        </div>
      </div>
      <ShotIn />
    </AbsoluteFill>
  );
};

const ShotRouter: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const p = L.f4.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <RouterVisual L={L} base={base} dur={dur} />
      <RouterSfx press={p[1] - 30} cutToBlock={p[1] + 4} />
    </AbsoluteFill>
  );
};

export const FirstMinutes: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.f1.p[1] - 10; // cities -> islands at "Then the global routes vanish"
  const b = L.f2.from - 6;
  const c = L.f3.from - 6;
  const d = L.f4.from - 6;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="cities">
        <ShotCities dur={a} labelAt={L.f1.p[0] - 10} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="islands">
        <ShotIslands dur={b - a} cutAt={16} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="street">
        <ShotStreet L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="gps">
        <ShotGPS L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="router">
        <ShotRouter L={L} base={d} dur={s.duration - d} />
      </Sequence>
    </AbsoluteFill>
  );
};
