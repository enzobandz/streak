import React, {useLayoutEffect, useRef} from 'react';
import * as THREE from 'three';
import {AbsoluteFill, Sequence, useCurrentFrame, useVideoConfig} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {ATM, CardTerminal} from '../components/three/Props';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {useLines, useScene} from '../lib/timeline';
import {CardRow, IconCard, ShotIn, Stat, Tag} from '../components/ui/Overlays';
import {Icon} from '../components/ui/Icons';
import {easeOut, pop, prog, rnd} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

/* ---- Shot A: coins & cards raining, then freezing ---- */
const ShotRain: React.FC<{dur: number; freezeAt: number}> = ({dur, freezeAt}) => {
  const f = useCurrentFrame();
  const t = Math.min(f, freezeAt) + Math.max(0, f - freezeAt) * 0.03; // time slows to a near-stop
  return (
    <AbsoluteFill>
      <Stage bg="#121a3d" fog={['#121a3d', 10, 30]} cam={[{f: 0, pos: [0, 1, 12], target: [0, 1, 0], fov: 40}, {f: dur, pos: [0, 0.6, 9], target: [0, 1, 0], fov: 40}]}>
        <Lights intensity={1.1} sunPos={[4, 8, 6]} />
        {Array.from({length: 70}).map((_, i) => {
          const card = i % 4 === 0;
          const x = (rnd('mx' + i) - 0.5) * 16;
          const z = (rnd('mz' + i) - 0.5) * 6;
          const speed = 0.05 + rnd('ms' + i) * 0.05;
          const y = 9 - ((rnd('my' + i) * 14 + t * speed) % 14);
          const rot = t * (0.05 + rnd('mr' + i) * 0.08);
          return card ? (
            <mesh key={i} position={[x, y, z]} rotation={[rot, rot * 0.7, 0.3]}>
              <boxGeometry args={[0.85, 0.54, 0.03]} />
              <meshStandardMaterial color={['#6c8cff', '#2ec4b6', '#f78fb3'][i % 3]} metalness={0.3} roughness={0.3} />
            </mesh>
          ) : (
            <mesh key={i} position={[x, y, z]} rotation={[rot + 1, rot * 0.5, 0]}>
              <cylinderGeometry args={[0.3, 0.3, 0.06, 24]} />
              <meshStandardMaterial color="#f3c14b" metalness={0.8} roughness={0.25} />
            </mesh>
          );
        })}
      </Stage>
      <AbsoluteFill style={{background: '#7fb8ff', mixBlendMode: 'saturation', opacity: 0.85 * prog(f, freezeAt, freezeAt + 12)}} />
      <Sfx name="coin" at={8} volume={0.3} />
      <Sfx name="coin" at={26} volume={0.25} />
      <Sfx name="glitch" at={freezeAt} volume={0.35} />
      <Sfx name="boom" at={freezeAt} volume={0.45} />
    </AbsoluteFill>
  );
};

/* ---- Shot B: café card terminal ---- */
const StatusPanel: React.FC<{rows: {icon: string; text: string; col: string; at: number}[]}> = ({rows}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  return (
    <div style={{position: 'absolute', right: 90, top: 250, width: 640, display: 'flex', flexDirection: 'column', gap: 22}}>
      {rows.map((r, i) => {
        const s = pop(f, fps, r.at);
        return (
          <div key={i} style={{display: 'flex', alignItems: 'center', gap: 20, padding: '22px 26px', borderRadius: 20, background: 'rgba(7,11,30,0.82)', border: `2px solid ${r.col}88`, opacity: Math.min(1, s), transform: `translateX(${(1 - s) * 60}px)`}}>
            <Icon name={r.icon} size={52} color={r.col} stroke={2.4} />
            <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 32, color: C.white}}>{r.text}</div>
          </div>
        );
      })}
    </div>
  );
};

const ShotTerminal: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.m2.p.map((x) => x - base);
  const st = f < 40 ? 'idle' : f < p[0] ? 'connecting' : f < p[1] + 20 ? 'offline' : 'limit';
  const screen = st === 'idle' ? '#7fe7ff' : st === 'connecting' ? (Math.floor(f / 10) % 2 ? '#6c8cff' : '#3c56b8') : st === 'offline' ? C.amber : C.alert;
  const cust = lookFor('cust');
  const barista = lookFor('barista2');
  return (
    <AbsoluteFill>
      <Stage bg="#f6e7d6" shadows cam={[{f: 0, pos: [1.6, 2.4, 4.4], target: [-0.8, 1.2, 0.2], fov: 36}, {f: dur, pos: [1.1, 2.1, 3.4], target: [-0.8, 1.15, 0.2], fov: 36}]}>
        <Lights intensity={1} sun="#ffe9cc" shadow shadowSize={6} sunPos={[2, 6, 4]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <planeGeometry args={[30, 30]} />
          <meshStandardMaterial color="#b98b62" />
        </mesh>
        <mesh position={[0, 3, -2.5]}>
          <planeGeometry args={[30, 6]} />
          <meshStandardMaterial color="#e9d3b8" />
        </mesh>
        {/* shelves with cups */}
        {[1.8, 2.5].map((y) => (
          <group key={y}>
            <mesh position={[0, y, -2.35]}>
              <boxGeometry args={[4, 0.06, 0.3]} />
              <meshStandardMaterial color="#7a5435" />
            </mesh>
            {Array.from({length: 7}).map((_, i) => (
              <mesh key={i} position={[-1.6 + i * 0.52, y + 0.13, -2.3]}>
                <cylinderGeometry args={[0.1, 0.08, 0.2, 12]} />
                <meshStandardMaterial color={['#ff6b6b', '#4ecdc4', '#ffd166', '#f4f5fa'][i % 4]} />
              </mesh>
            ))}
          </group>
        ))}
        {/* counter */}
        <mesh position={[0, 0.5, 0]} castShadow receiveShadow>
          <boxGeometry args={[4.5, 1, 1]} />
          <meshStandardMaterial color="#3d5a80" />
        </mesh>
        <mesh position={[0, 1.02, 0]} receiveShadow>
          <boxGeometry args={[4.7, 0.06, 1.1]} />
          <meshStandardMaterial color="#efe6da" />
        </mesh>
        <CardTerminal position={[-0.6, 1.05, 0.15]} screen={screen} rotation={0.4} />
        <Person look={barista} position={[0.6, 0, -1.2]} rotation={-0.2} pose={{mood: st === 'limit' ? -0.5 : 0.6, blink: isBlink(f, 'ba'), shrug: st === 'limit' ? prog(f, p[1] + 20, p[1] + 35) : 0}} />
        <Person
          look={cust}
          position={[-2.0, 0, 1.0]}
          rotation={2.0}
          pose={{armR: -0.9 * prog(f, 10, 35), hold: 'none', mood: st === 'limit' ? -0.6 : 0.5, blink: isBlink(f, 'cu')}}
        />
        <mesh position={[-0.75, 1.32, 0.55]} rotation={[-0.6, 0.4, 0]} scale={prog(f, 25, 35)}>
          <boxGeometry args={[0.34, 0.22, 0.01]} />
          <meshStandardMaterial color="#6c8cff" metalness={0.3} roughness={0.3} />
        </mesh>
      </Stage>
      <StatusPanel
        rows={[
          {icon: 'bank', text: 'Card asks the bank: “Approve?”', col: C.data, at: 30},
          {icon: 'wifiOff', text: 'No answer from the bank', col: C.alert, at: p[0] - 50},
          {icon: 'clock', text: 'Some terminals store it to send later', col: C.amber, at: p[0] + 6},
          {icon: 'alert', text: '…but only up to set limits', col: C.alert, at: p[1] + 10},
        ]}
      />
      <ShotIn />
      <Sfx name="blip" at={40} volume={0.35} />
      <Sfx name="error" at={p[0] - 50} volume={0.3} />
      <Sfx name="chime" at={p[0] + 6} volume={0.3} />
      <Sfx name="error" at={p[1] + 10} volume={0.4} />
    </AbsoluteFill>
  );
};

/* ---- Shot C: digital money down, ATM, cash is king ---- */
const ShotCash: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const p = L.m3.p.map((x) => x - base);
  const cardsOut = prog(f, p[1] + 10, p[1] + 24);
  const atmDown = f > p[2] - 10;
  const king = p[4];
  const look = lookFor('cashguy');
  return (
    <AbsoluteFill>
      <Stage
        bg="#1d2a52"
        fog={['#1d2a52', 12, 34]}
        shadows
        cam={[
          {f: 0, pos: [0, 2.2, 9], target: [0, 1.4, 0], fov: 38},
          {f: p[1] + 20, pos: [0, 2.2, 9], target: [0, 1.4, 0], fov: 38},
          {f: king, pos: [2.2, 1.8, 5.5], target: [0.6, 1.3, 0], fov: 38},
          {f: dur, pos: [2.6, 1.7, 4.8], target: [0.7, 1.4, 0], fov: 38},
        ]}
      >
        <Lights intensity={0.9} shadow shadowSize={8} sunPos={[4, 9, 6]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <planeGeometry args={[40, 40]} />
          <meshStandardMaterial color="#7b86a6" />
        </mesh>
        <mesh position={[0, 3, -1.2]} receiveShadow>
          <boxGeometry args={[12, 6, 0.4]} />
          <meshStandardMaterial color="#c9b79c" />
        </mesh>
        <ATM position={[-0.6, 0, -0.6]} screen={atmDown ? C.alert : '#7fe7ff'} />
        <Person
          look={look}
          position={[1.0, 0, 0.6]}
          rotation={-0.5}
          pose={{
            hold: f > king ? 'cash' : 'none',
            armR: f > king ? -1.3 * prog(f, king, king + 12, easeOut) : 0,
            mood: f > king ? 0.9 : atmDown ? -0.6 : 0.2,
            blink: isBlink(f, 'cg'),
            headYaw: f > king ? 0.4 : -0.6,
          }}
        />
      </Stage>
      <div style={{opacity: 1 - cardsOut}}>
        <CardRow y={150} gap={34}>
          <IconCard icon="bank" label="Online banking" state="down" at={0} stateAt={p[0] - 6} />
          <IconCard icon="phone" label="Mobile wallets" state="down" at={p[0]} stateAt={p[0] + 40} />
          <IconCard icon="cash" label="Instant transfers" state="down" at={p[0] + 14} stateAt={p[1] - 20} />
        </CardRow>
      </div>
      <Tag text="Out of service" icon="x" x={700} y={230} at={p[2] - 6} until={king} color={C.alert} />
      <div style={{position: 'absolute', left: 120, top: 150, transform: `scale(${pop(f, fps, king, 10)}) rotate(-4deg)`, transformOrigin: 'left center', opacity: Math.min(1, pop(f, fps, king, 10))}}>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 130, color: '#7cc98a', textShadow: '0 0 40px #7cc98a66, 0 8px 30px #000'}}>CASH IS KING</div>
        <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 36, color: C.white, opacity: prog(f, p[5], p[5] + 12)}}>…for anyone who has some.</div>
      </div>
      <ShotIn />
      <Sfx name="error" at={p[0] - 6} volume={0.3} />
      <Sfx name="error" at={p[0] + 40} volume={0.25} />
      <Sfx name="error" at={p[1] - 20} volume={0.25} />
      <Sfx name="powerDown" at={p[2] - 10} volume={0.35} />
      <Sfx name="coin" at={king} volume={0.5} />
      <Sfx name="hit" at={king} volume={0.3} />
    </AbsoluteFill>
  );
};

/* ---- Shot D: trading board freezes ---- */
const ShotMarket: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const p = L.m4.p.map((x) => x - base);
  const halt = p[1] + 8;
  const t = Math.min(f, halt);
  const bars = 28;
  return (
    <AbsoluteFill>
      <Stage bg="#0a1029" cam={[{f: 0, pos: [-6, 3, 13], target: [0, 3, 0], fov: 40}, {f: dur, pos: [5, 3, 12], target: [0, 3, 0], fov: 40}]}>
        <ambientLight intensity={0.4} />
        <pointLight position={[0, 6, 8]} intensity={80} color="#bcd7ff" />
        <mesh position={[0, 3.5, -0.3]}>
          <boxGeometry args={[18, 7.4, 0.3]} />
          <meshStandardMaterial color="#141b3a" />
        </mesh>
        {Array.from({length: bars}).map((_, i) => {
          const h = 1 + 2.4 * (0.5 + 0.5 * Math.sin(t * 0.08 + i * 0.7) * Math.cos(t * 0.03 + i * 0.3));
          const up = Math.sin(t * 0.08 + i * 0.7 + 0.2) > Math.sin(t * 0.08 + i * 0.7);
          const col = f > halt ? '#5a6385' : up ? C.ok : C.alert;
          return (
            <mesh key={i} position={[-8 + i * 0.6, 0.6 + h / 2, 0]}>
              <boxGeometry args={[0.4, h, 0.3]} />
              <meshBasicMaterial color={col} toneMapped={false} />
            </mesh>
          );
        })}
        {/* ticker line */}
        <mesh position={[0, 6.4, 0.05]}>
          <planeGeometry args={[17.4, 0.8]} />
          <meshBasicMaterial color={f > halt ? C.alert : '#1e2a5a'} toneMapped={false} />
        </mesh>
      </Stage>
      <div style={{position: 'absolute', left: 0, right: 0, top: 230, overflow: 'hidden', height: 60, opacity: f > halt ? 0 : 1}}>
        <div style={{whiteSpace: 'nowrap', fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 40, color: C.white, transform: `translateX(${-t * 6}px)`}}>
          {Array.from({length: 12})
            .map((_, i) => `IDX${i + 1} ${i % 3 ? '▲' : '▼'} ${(rnd('tk' + i) * 3).toFixed(2)}%`)
            .join('      ')}
        </div>
      </div>
      <Tag text="Exchanges: private networks" icon="server" x={480} y={900} at={p[0] - 70} until={halt} color={C.data} />
      <Tag text="Brokers & investors: offline" icon="users" x={1440} y={900} at={p[0] + 20} until={halt} color={C.alert} />
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>
        <div style={{transform: `scale(${pop(f, fps, halt, 10)})`, padding: '22px 60px', background: C.alert, borderRadius: 18, fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 110, color: C.white, letterSpacing: 4, boxShadow: `0 0 80px ${C.alert}aa`}}>
          TRADING HALTED
        </div>
      </AbsoluteFill>
      <ShotIn />
      <Sfx name="typing" at={10} volume={0.25} />
      <Sfx name="hit" at={halt} volume={0.5} />
    </AbsoluteFill>
  );
};

/* ---- Shot E: $110T a year -> ~$300B a day ---- */
const DaySlabs: React.FC<{highlight: number; dim: number; split: number}> = ({highlight, dim, split}) => {
  const ref = useRef<THREE.InstancedMesh>(null);
  useLayoutEffect(() => {
    const m = ref.current;
    if (!m) return;
    const d = new THREE.Object3D();
    const c = new THREE.Color();
    for (let i = 0; i < 365; i++) {
      const col = i % 73, row = Math.floor(i / 73);
      d.position.set((col - 36) * 0.2 * (1 + split * 0.25), row * 0.85 * (1 + split * 0.3), 0);
      d.scale.set(1, 1, 1);
      d.updateMatrix();
      m.setMatrixAt(i, d.matrix);
      const isDay = i === 182;
      c.set(isDay && highlight > 0 ? C.amber : '#3fbf7f').multiplyScalar(isDay ? 1 + highlight : 0.55 + 0.45 * (1 - dim * 0.6));
      m.setColorAt(i, c);
    }
    m.instanceMatrix.needsUpdate = true;
    if (m.instanceColor) m.instanceColor.needsUpdate = true;
  });
  return (
    <instancedMesh ref={ref} args={[undefined, undefined, 365]}>
      <boxGeometry args={[0.16, 0.75, 0.6]} />
      <meshStandardMaterial emissive="#0c2a1a" roughness={0.5} />
    </instancedMesh>
  );
};

const ShotGDP: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.m5.p.map((x) => x - base);
  const split = prog(f, p[1] - 30, p[1]);
  const hi = prog(f, p[1], p[1] + 20);
  return (
    <AbsoluteFill>
      <Stage
        bg="#0b1530"
        cam={[
          {f: 0, pos: [0, 8, 24], target: [0, 1.5, 0], fov: 40},
          {f: p[1] - 30, pos: [-4, 5, 18], target: [0, 1.8, 0], fov: 40},
          {f: p[1] + 40, pos: [2, 2.8, 5.5], target: [0, 2.3, 0], fov: 40},
          {f: p[2], pos: [2, 2.8, 5.5], target: [0, 2.3, 0], fov: 40},
          {f: dur, pos: [0, 9, 27], target: [0, 1.5, 0], fov: 40},
        ]}
      >
        <Lights intensity={0.9} sunPos={[6, 12, 10]} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.4, 0]}>
          <circleGeometry args={[30, 64]} />
          <meshStandardMaterial color="#131d40" />
        </mesh>
        <DaySlabs highlight={hi} dim={prog(f, p[2], p[3] + 20)} split={split} />
      </Stage>
      <Stat value="$110 trillion" label="world economic output per year (IMF, 2024)" at={p[0] + 30} until={p[1] - 4} x={110} y={110} size={120} color={C.ok} />
      <Stat value="≈ $300 billion" label="every single day (110 trillion ÷ 365)" at={p[1]} until={p[2] + 4} x={110} y={110} size={120} color={C.amber} />
      <Stat value="Not all would stop…" label="…but a large share now depends on connected systems" at={p[2] + 6} x={110} y={110} size={84} color={C.white} />
      <ShotIn />
      <Sfx name="whoosh" at={p[1] - 30} volume={0.35} />
      <Sfx name="blip" at={p[1]} volume={0.4} />
    </AbsoluteFill>
  );
};

export const Money: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.m2.from - 8, b = L.m3.from - 8, c = L.m4.from - 8, d = L.m5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="rain">
        <ShotRain dur={a} freezeAt={L.m1.end - 8} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="terminal">
        <ShotTerminal L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="cash">
        <ShotCash L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="market">
        <ShotMarket L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="gdp">
        <ShotGDP L={L} base={d} dur={s.duration - d} />
      </Sequence>
    </AbsoluteFill>
  );
};
