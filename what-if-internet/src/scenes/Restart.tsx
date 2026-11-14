import React, {useLayoutEffect, useMemo, useRef} from 'react';
import * as THREE from 'three';
import {AbsoluteFill, Sequence, useCurrentFrame} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {BigSwitch, ServerRack} from '../components/three/Props';
import {buildGraph, NetworkGraph} from '../components/three/Network';
import {Globe, Stars} from '../components/three/Globe';
import {useLines, useScene} from '../lib/timeline';
import {CardRow, Headline, IconCard, ShotIn, Stat, Tag} from '../components/ui/Overlays';
import {clamp, easeOut, prog, rnd} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const ShotSwitchOn: React.FC<{L: LinesT; dur: number}> = ({L, dur}) => {
  const f = useCurrentFrame();
  const flip = L.r1.p[0] - 6;
  const on = prog(f, flip, flip + 8);
  return (
    <AbsoluteFill>
      <Stage bg="#0b1129" fog={['#0b1129', 14, 40]} shadows cam={[{f: 0, pos: [-4, 3, 9], target: [0, 1.8, 0], fov: 38}, {f: dur, pos: [-1.6, 2.2, 6], target: [0, 1.8, 0], fov: 38}]}>
        <Lights intensity={0.6 + on * 0.5} shadow shadowSize={8} sunPos={[4, 10, 6]} />
        <spotLight position={[0, 9, 4]} angle={0.5} penumbra={0.6} intensity={20 + on * 60} color={on > 0.5 ? '#c9ffe2' : '#ffc9d3'} />
        <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
          <circleGeometry args={[40, 64]} />
          <meshStandardMaterial color="#141c3d" />
        </mesh>
        <gridHelper args={[60, 60, '#2a3a74', '#1d2856']} position={[0, 0.01, 0]} />
        <BigSwitch position={[0, 1.9, 0]} on={on} />
      </Stage>
      <Tag text="Routes come back" icon="power" x={960} y={150} at={flip + 4} until={L.r1.p[1] + 4} color={C.ok} />
      <Tag text="…but not like flicking on a light" icon="alert" x={960} y={150} at={L.r1.p[1] + 4} color={C.amber} />
      <Sfx name="switch" at={flip} volume={0.7} />
      <Sfx name="powerUp" at={flip + 2} volume={0.5} />
    </AbsoluteFill>
  );
};

const ShotRebuild: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const graph = useMemo(() => buildGraph('islands', 8, 9, 7), []);
  const p = L.r2.p.map((x) => x - base);
  const conn = prog(f, 20, dur - 10, (x) => x);
  return (
    <AbsoluteFill>
      <Stage bg="#080d24" fog={['#080d24', 20, 50]} cam={[{f: 0, pos: [6, 18, 22], target: [0, 0, 0], fov: 42}, {f: dur, pos: [0, 12, 19], target: [0, 0, 0], fov: 40}]}>
        <ambientLight intensity={0.6} />
        <pointLight position={[0, 8, 6]} intensity={40} color="#9fd3ff" />
        <NetworkGraph frame={f} graph={graph} connectivity={conn} islandSpread={1.3 * (1 - conn)} exchange={prog(f, p[0], p[1])} linkReveal={1} />
      </Stage>
      <Tag text="Networks re-announce their routes" icon="network" x={960} y={150} at={8} until={p[0] + 6} color={C.data} />
      <Tag text="The map rebuilds… piece by piece" icon="map" x={960} y={150} at={p[0] + 6} color={C.amber} />
      <ShotIn />
      {Array.from({length: 8}).map((_, i) => (
        <Sfx key={i} name="blip" at={20 + i * 18} volume={0.18} />
      ))}
    </AbsoluteFill>
  );
};

/* Thundering herd: thousands of requests converge on one data centre */
const Herd: React.FC<{frame: number; start: number}> = ({frame, start}) => {
  const ref = useRef<THREE.InstancedMesh>(null);
  const N = 1400;
  const seeds = useMemo(
    () =>
      Array.from({length: N}).map((_, i) => {
        const th = rnd('ht' + i) * Math.PI * 2;
        const ph = Math.acos(rnd('hp' + i) * 2 - 1);
        return {dir: new THREE.Vector3(Math.sin(ph) * Math.cos(th), Math.abs(Math.cos(ph)) * 0.7 + 0.1, Math.sin(ph) * Math.sin(th)), delay: rnd('hd' + i) * 90, speed: 0.6 + rnd('hs' + i) * 0.8};
      }),
    [],
  );
  useLayoutEffect(() => {
    const m = ref.current;
    if (!m) return;
    const d = new THREE.Object3D();
    seeds.forEach((s, i) => {
      const t = ((frame - start - s.delay) * 0.01 * s.speed) % 1;
      const live = frame - start - s.delay > 0;
      const r = 26 * (1 - (t < 0 ? 0 : t));
      d.position.copy(s.dir).multiplyScalar(r).add(new THREE.Vector3(0, 1.5, 0));
      d.scale.setScalar(live ? 0.12 : 0);
      d.updateMatrix();
      m.setMatrixAt(i, d.matrix);
    });
    m.instanceMatrix.needsUpdate = true;
  });
  return (
    <instancedMesh ref={ref} args={[undefined, undefined, N]}>
      <sphereGeometry args={[1, 6, 4]} />
      <meshBasicMaterial color="#d6fbff" toneMapped={false} />
    </instancedMesh>
  );
};

const LoadChart: React.FC<{p: number; spike: number}> = ({p, spike}) => {
  const pts = Array.from({length: 60}, (_, i) => {
    const x = i / 59;
    const v = x < spike ? 0.1 + Math.sin(i) * 0.02 : Math.min(1, 0.1 + (x - spike) * 6) * (0.92 + Math.sin(i * 1.7) * 0.06);
    return `${x * 520},${200 - v * 180}`;
  }).slice(0, Math.max(2, Math.floor(60 * p)));
  return (
    <div style={{position: 'absolute', right: 110, bottom: 120, padding: 24, background: 'rgba(7,11,30,0.82)', borderRadius: 20, border: `2px solid ${C.alert}66`}}>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 26, color: C.mist, marginBottom: 10}}>Requests per second (illustrative)</div>
      <svg width={520} height={210}>
        <line x1={0} y1={20} x2={520} y2={20} stroke={C.alert} strokeDasharray="6 6" strokeWidth={2} />
        <text x={520} y={14} textAnchor="end" fill={C.alert} fontFamily="Inter" fontSize={18}>
          normal peak
        </text>
        <polyline points={pts.join(' ')} fill="none" stroke={C.alert} strokeWidth={4} />
      </svg>
    </div>
  );
};

const ShotHerd: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.r3.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage bg="#060a1c" fog={['#060a1c', 20, 60]} cam={[{f: 0, pos: [0, 10, 30], target: [0, 1.5, 0], fov: 42}, {f: dur, pos: [12, 6, 18], target: [0, 1.5, 0], fov: 42}]}>
        <ambientLight intensity={0.4} />
        <pointLight position={[0, 6, 4]} intensity={60} color={C.alert} />
        <Stars count={600} opacity={0.4} />
        {[-1.4, 0, 1.4].map((x, i) => (
          <ServerRack key={x} position={[x, 0, 0]} frame={f} seed={'h' + i} ledColor={f > p[0] ? C.alert : C.data} />
        ))}
        <Herd frame={f} start={10} />
      </Stage>
      <Headline text="Billions of devices reconnect at once" at={6} until={p[0] + 10} y={110} size={76} center />
      <CardRow y={120} gap={26}>
        {f > p[0] ? (
          <>
            <IconCard icon="phone" label="Phones syncing" at={p[0] + 6} width={260} />
            <IconCard icon="spinner" label="Apps retrying" at={p[1]} width={260} />
            <IconCard icon="card" label="Queued payments" at={p[2]} width={260} />
          </>
        ) : null}
      </CardRow>
      <LoadChart p={prog(f, 0, dur * 0.8, (x) => x)} spike={0.25} />
      <div style={{position: 'absolute', left: 110, bottom: 140, opacity: prog(f, p[3], p[3] + 12, easeOut)}}>
        <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 32, color: C.mist}}>Engineers call it a</div>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 110, color: C.alert, textShadow: `0 0 40px ${C.alert}66`}}>Thundering Herd</div>
      </div>
      <ShotIn />
      <Sfx name="riser" at={10} volume={0.3} />
      <Sfx name="pop" at={p[0] + 6} volume={0.25} />
      <Sfx name="pop" at={p[1]} volume={0.25} />
      <Sfx name="pop" at={p[2]} volume={0.25} />
      <Sfx name="boom" at={p[3]} volume={0.55} />
    </AbsoluteFill>
  );
};

const Gauge: React.FC<{v: number; warn: boolean}> = ({v, warn}) => {
  const a = -120 + v * 240;
  const col = warn ? C.alert : C.ok;
  return (
    <div style={{position: 'absolute', right: 120, top: 230, padding: 24, background: 'rgba(7,11,30,0.82)', borderRadius: 24, border: `2px solid ${col}66`, textAlign: 'center'}}>
      <svg width={300} height={200} viewBox="-150 -150 300 200">
        <path d="M -120 0 A 120 120 0 1 1 120 0" fill="none" stroke="rgba(255,255,255,0.12)" strokeWidth={22} transform="rotate(30)" />
        <path d="M 60 -104 A 120 120 0 0 1 120 0" fill="none" stroke={C.alert} strokeWidth={22} transform="rotate(30)" opacity={0.8} />
        <line x1={0} y1={0} x2={Math.sin((a * Math.PI) / 180) * 100} y2={-Math.cos((a * Math.PI) / 180) * 100} stroke={C.white} strokeWidth={8} strokeLinecap="round" />
        <circle r={12} fill={C.white} />
      </svg>
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 28, color: col}}>{warn ? 'Power surge risk' : 'Data-centre power draw'}</div>
    </div>
  );
};

const ShotDataCenter: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.r4.p.map((x) => x - base);
  const warnFrom = p[0] + 10;
  const gradual = p[2] + 4;
  const allOn = f > warnFrom && f < gradual;
  const stage = clamp((f - gradual) / 90);
  const rows = 4, cols = 7;
  const gaugeV = f < warnFrom ? 0.15 : f < gradual ? 0.15 + 0.8 * prog(f, warnFrom, warnFrom + 50) : 0.15 + 0.5 * stage;
  return (
    <AbsoluteFill>
      <Stage bg="#0a0f24" fog={['#0a0f24', 10, 34]} cam={[{f: 0, pos: [-6, 4, 12], target: [0, 1, -4], fov: 42}, {f: dur, pos: [5, 3.2, 10], target: [0, 1, -4], fov: 42}]}>
        <ambientLight intensity={0.3} />
        <pointLight position={[0, 5, 2]} intensity={40} color="#9fb8ff" />
        <mesh rotation={[-Math.PI / 2, 0, 0]}>
          <planeGeometry args={[60, 60]} />
          <meshStandardMaterial color="#1c2238" />
        </mesh>
        {Array.from({length: rows * cols}).map((_, i) => {
          const r = Math.floor(i / cols);
          const section = r / rows;
          const pw = f < warnFrom ? 0.05 : allOn ? 0.05 + 0.2 * Math.round(rnd('fl' + i + Math.floor(f / 6))) : stage > section ? 1 : 0.05;
          return <ServerRack key={i} position={[(i % cols) * 1.2 - 3.6, 0, -r * 2.6]} frame={f} power={pw} seed={'dc' + i} />;
        })}
      </Stage>
      <Headline text="Facebook, October 2021" sub="Offline for about six hours" at={6} until={warnFrom + 30} y={110} size={80} />
      <Gauge v={gaugeV} warn={f > warnFrom + 30 && f < gradual} />
      <Tag text="Switching everything on at once → power surges" icon="bolt" x={760} y={160} at={warnFrom + 30} until={gradual} color={C.alert} size={30} />
      <Tag text="So they brought services back gradually" icon="check" x={760} y={160} at={gradual} color={C.ok} size={30} />
      <ShotIn />
      <Sfx name="hum" at={warnFrom} volume={0.35} />
      <Sfx name="error" at={warnFrom + 30} volume={0.3} />
      {[0, 1, 2, 3].map((k) => (
        <Sfx key={k} name="switch" at={gradual + k * 22} volume={0.35} />
      ))}
    </AbsoluteFill>
  );
};

const Backlog: React.FC<{label: string; icon: string; v: number}> = ({label, v}) => (
  <div style={{marginBottom: 20}}>
    <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 28, color: C.white, marginBottom: 8}}>{label}</div>
    <div style={{width: 560, height: 22, borderRadius: 11, background: 'rgba(255,255,255,0.1)', overflow: 'hidden'}}>
      <div style={{width: `${v * 100}%`, height: '100%', background: `linear-gradient(90deg, ${C.amber}, ${C.alert})`}} />
    </div>
  </div>
);

const ShotBacklog: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.r5.p.map((x) => x - base);
  const t = prog(f, 0, dur, (x) => x);
  return (
    <AbsoluteFill>
      <Stage bg="#040714" cam={[{f: 0, pos: [0, 0.8, 7], target: [-1.6, 0, 0], fov: 36}, {f: dur, pos: [0, 1, 6.4], target: [-1.6, 0, 0], fov: 36}]}>
        <ambientLight intensity={0.25} />
        <directionalLight position={[-6, 3, 6]} intensity={1.6} color="#bcd7ff" />
        <Stars />
        <Globe frame={f} rotation={1.2 - f * 0.003} connectivity={0.2 + 0.8 * t} />
      </Stage>
      <div style={{position: 'absolute', left: 110, top: 300, padding: 34, background: 'rgba(7,11,30,0.8)', borderRadius: 24, opacity: prog(f, 6, 20)}}>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 40, color: C.white, marginBottom: 22}}>Backlogs clearing…</div>
        <Backlog label="Queued payments" icon="card" v={1 - t * 0.25} />
        <Backlog label="Rescheduled flights" icon="plane" v={1 - t * 0.12} />
        <Backlog label="Delayed shipments" icon="truck" v={1 - t * 0.08} />
      </div>
      <Stat value="Hours… maybe days" label="of slow services and backlogs" at={p[0]} x={110} y={110} size={92} color={C.amber} />
      <ShotIn />
    </AbsoluteFill>
  );
};

export const Restart: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.r2.from - 8, b = L.r3.from - 8, c = L.r4.from - 8, d = L.r5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="switchOn">
        <ShotSwitchOn L={L} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="rebuild">
        <ShotRebuild L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="herd">
        <ShotHerd L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="datacentre">
        <ShotDataCenter L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="backlog">
        <ShotBacklog L={L} base={d} dur={s.duration - d} />
      </Sequence>
    </AbsoluteFill>
  );
};
