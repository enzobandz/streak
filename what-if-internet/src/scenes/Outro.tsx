import React from 'react';
import {AbsoluteFill, Sequence, useCurrentFrame, useVideoConfig} from 'remotion';
import {Lights, Stage} from '../components/three/Stage';
import {Skyline, Street} from '../components/three/City';
import {Globe, Stars} from '../components/three/Globe';
import {Plane} from '../components/three/Props';
import {isBlink, lookFor, Person} from '../components/three/Person';
import {useLines, useScene} from '../lib/timeline';
import {CardRow, Headline, IconCard, ShotIn} from '../components/ui/Overlays';
import {Icon} from '../components/ui/Icons';
import {Logo} from '../components/ui/Logo';
import {easeOut, pop, prog} from '../lib/anim';
import {C, FONT_BODY, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

type LinesT = ReturnType<typeof useLines>;

const ShotDawn: React.FC<{L: LinesT; dur: number}> = ({L, dur}) => {
  const f = useCurrentFrame();
  const p = L.o1.p;
  const sun = prog(f, 0, dur);
  return (
    <AbsoluteFill>
      <Stage bg="#ffc9a0" fog={['#ffc9a0', 50, 150]} shadows cam={[{f: 0, pos: [44, 24, 54], target: [0, 2, 0], fov: 34}, {f: dur, pos: [36, 18, 46], target: [0, 4, 0], fov: 34}]}>
        <Lights intensity={0.7 + 0.4 * sun} sun="#ffc08a" sky="#ffe0c2" shadow shadowSize={50} sunPos={[-40, 10 + sun * 16, 10]} />
        <Skyline size={5} seed="dawn" frame={f} night={0.6 - 0.6 * sun} ground="#94ae8c" />
        <Plane position={[30 - f * 0.12, 16 - f * 0.02, -10]} rotation={[0, Math.PI, 0.05]} scale={1.4} livery={C.amber} />
      </Stage>
      <Headline text="Civilization doesn't end" at={L.o1.from - 6} until={p[0] + 6} y={110} size={84} center />
      <CardRow y={680} gap={30}>
        <IconCard icon="water" label="Water flows" state="ok" at={p[0]} stateAt={p[0] + 14} />
        <IconCard icon="bolt" label="Lights stay on" state="ok" at={p[1]} stateAt={p[1] + 14} />
        <IconCard icon="plane" label="Planes land" state="ok" at={p[2]} stateAt={p[2] + 14} />
      </CardRow>
      <Sfx name="chime" at={p[0] + 14} volume={0.25} />
      <Sfx name="chime" at={p[1] + 14} volume={0.25} />
      <Sfx name="chime" at={p[2] + 14} volume={0.25} />
    </AbsoluteFill>
  );
};

const ShotAgreements: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.o2.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage bg="#040714" cam={[{f: 0, pos: [0, 0.6, 5.2], target: [0, 0, 0], fov: 38}, {f: dur, pos: [0, 0.9, 6.4], target: [0, 0, 0], fov: 38}]}>
        <ambientLight intensity={0.25} />
        <directionalLight position={[-6, 3, 6]} intensity={1.6} color="#bcd7ff" />
        <Stars />
        <Globe frame={f + 500} rotation={-0.3 - f * 0.003} connectivity={1} />
      </Stage>
      <Headline text="Invisible agreements between machines" at={10} until={p[0] + 4} y={110} size={78} center />
      <Headline text="How many of us have a plan B?" at={p[0] + 4} y={110} size={84} center color={C.amber} />
      <ShotIn />
    </AbsoluteFill>
  );
};

const CheckItem: React.FC<{icon: string; text: string; at: number}> = ({icon, text, at}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const s = pop(f, fps, at);
  const tick = pop(f, fps, at + 10, 9);
  return (
    <div style={{display: 'flex', alignItems: 'center', gap: 24, marginBottom: 26, opacity: Math.min(1, s), transform: `translateX(${(1 - s) * 50}px)`}}>
      <div style={{width: 56, height: 56, borderRadius: 14, border: `3px solid ${C.ok}`, display: 'flex', alignItems: 'center', justifyContent: 'center', background: tick > 0.5 ? C.ok : 'transparent'}}>
        <div style={{transform: `scale(${tick})`}}>
          <Icon name="check" size={40} color={C.ink} stroke={3.4} />
        </div>
      </div>
      <Icon name={icon} size={52} color={C.white} />
      <div style={{fontFamily: FONT_BODY, fontWeight: 600, fontSize: 42, color: C.white}}>{text}</div>
    </div>
  );
};

const ShotPlanB: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const p = L.o3.p.map((x) => x - base);
  const wave = Math.max(0, f - p[3]);
  return (
    <AbsoluteFill>
      <Stage bg="#ffc79a" fog={['#ffc79a', 30, 90]} shadows cam={[{f: 0, pos: [-6, 3, 12], target: [-3, 1.5, 3], fov: 38}, {f: dur, pos: [-5, 2.6, 10], target: [-3, 1.4, 4], fov: 38}]}>
        <Lights intensity={0.95} sun="#ffc59a" shadow shadowSize={20} sunPos={[-14, 10, 12]} />
        <Street frame={f * 0.3} seed="planb" nearSide={false} length={60} />
        <Person look={lookFor('pb1')} position={[-4.4, 0.06, 4.4]} rotation={1.2} pose={{mood: 0.9, blink: isBlink(f, 'pb1'), armR: wave > 0 ? -2.6 + Math.sin(wave * 0.4) * 0.3 : 0}} />
        <Person look={lookFor('pb2')} position={[-2.4, 0.06, 4.6]} rotation={-1.2} pose={{mood: 0.9, blink: isBlink(f, 'pb2'), armL: wave > 0 ? -2.6 + Math.sin(wave * 0.4 + 1) * 0.3 : 0}} />
      </Stage>
      <div style={{position: 'absolute', right: 110, top: 220, padding: '40px 50px 20px', background: 'rgba(7,11,30,0.82)', borderRadius: 28}}>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 52, color: C.amber, marginBottom: 30}}>Your plan B</div>
        <CheckItem icon="cash" text="A little cash" at={p[0]} />
        <CheckItem icon="map" text="An offline map" at={p[1]} />
        <CheckItem icon="notebook" text="Key numbers on paper" at={p[2]} />
        <CheckItem icon="users" text="Know your neighbors" at={p[3]} />
      </div>
      <ShotIn />
      {[0, 1, 2, 3].map((k) => (
        <Sfx key={k} name="pop" at={p[k] + 10} volume={0.3} />
      ))}
    </AbsoluteFill>
  );
};

const ShotResilient: React.FC<{L: LinesT; base: number; dur: number}> = ({L, base, dur}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const p = L.o4.p.map((x) => x - base);
  return (
    <AbsoluteFill>
      <Stage bg="#040714" cam={[{f: 0, pos: [0, 0.5, 6], target: [0, 0, 0], fov: 38}, {f: dur, pos: [0, 1.6, 13], target: [0, 0.5, 0], fov: 38}]}>
        <ambientLight intensity={0.25} />
        <directionalLight position={[-6, 3, 6]} intensity={1.6} color="#bcd7ff" />
        <Stars />
        <Globe frame={f + 900} rotation={-1.4 - f * 0.004} connectivity={1} />
      </Stage>
      <Headline text="The internet is remarkably resilient." at={6} y={110} size={72} center />
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'flex-end', paddingBottom: 150}}>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 150, color: C.amber, transform: `scale(${pop(f, fps, p[0] + 20, 10)})`, textShadow: `0 0 60px ${C.amber}66`}}>Are we?</div>
      </AbsoluteFill>
      <ShotIn />
      <Sfx name="hit" at={p[0] + 20} volume={0.35} />
    </AbsoluteFill>
  );
};

const EndCard: React.FC<{L: LinesT; base: number}> = ({L, base}) => {
  const f = useCurrentFrame();
  const {fps} = useVideoConfig();
  const p = L.o5.p.map((x) => x - base);
  const sub = pop(f, fps, p[1], 10);
  const clicked = f > p[1] + 40;
  return (
    <AbsoluteFill>
      <Stage bg="#03050f" cam={[{f: 0, pos: [0, -1.4, 5], target: [0, 0.6, 0], fov: 40}, {f: 300, pos: [0, -1.5, 4.6], target: [0, 0.7, 0], fov: 40}]}>
        <ambientLight intensity={0.25} />
        <directionalLight position={[-4, 2, 5]} intensity={1.2} color="#bcd7ff" />
        <Stars />
        <Globe frame={f} rotation={-2 - f * 0.002} connectivity={1} tilt={0.5} />
      </Stage>
      <AbsoluteFill style={{background: 'linear-gradient(180deg, rgba(3,5,15,0.2), rgba(3,5,15,0.85))'}} />
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center'}}>
        <div style={{opacity: prog(f, 0, 15), transform: `translateY(${(1 - prog(f, 0, 20, easeOut)) * 30}px)`}}>
          <Logo size={1.6} />
        </div>
        <div style={{fontFamily: FONT_HEAD, fontWeight: 700, fontSize: 72, color: C.white, marginTop: 40, opacity: prog(f, 8, 24)}}>Thanks for watching</div>
        <div style={{display: 'flex', gap: 30, marginTop: 50}}>
          <div style={{transform: `scale(${sub * (clicked && f < p[1] + 46 ? 0.92 : 1)})`, padding: '22px 46px', borderRadius: 50, background: clicked ? '#3a3f55' : C.alert, fontFamily: FONT_BODY, fontWeight: 800, fontSize: 38, color: C.white, display: 'flex', alignItems: 'center', gap: 16}}>
            <Icon name="bell" size={40} color={C.white} />
            {clicked ? 'Subscribed' : 'Subscribe'}
          </div>
          <div style={{transform: `scale(${pop(f, fps, p[1] + 8, 10)})`, padding: '22px 46px', borderRadius: 50, border: `3px solid ${C.data}`, fontFamily: FONT_BODY, fontWeight: 600, fontSize: 38, color: C.white, display: 'flex', alignItems: 'center', gap: 16}}>
            <Icon name="comment" size={40} color={C.data} />
            What should we imagine next?
          </div>
        </div>
        <div style={{fontFamily: FONT_BODY, fontSize: 24, color: C.mist, marginTop: 60, opacity: prog(f, p[1] + 60, p[1] + 80)}}>
          Sources & methodology in the description · All animation, music and sound are original
        </div>
      </AbsoluteFill>
      <ShotIn />
      <Sfx name="pop" at={p[1]} volume={0.3} />
      <Sfx name="tick" at={p[1] + 40} volume={0.5} />
    </AbsoluteFill>
  );
};

export const Outro: React.FC = () => {
  const s = useScene();
  const L = useLines();
  const a = L.o2.from - 8, b = L.o3.from - 8, c = L.o4.from - 8, d = L.o5.from - 8;
  return (
    <AbsoluteFill>
      <Sequence durationInFrames={a} name="dawn">
        <ShotDawn L={L} dur={a} />
      </Sequence>
      <Sequence from={a} durationInFrames={b - a} name="agreements">
        <ShotAgreements L={L} base={a} dur={b - a} />
      </Sequence>
      <Sequence from={b} durationInFrames={c - b} name="planB">
        <ShotPlanB L={L} base={b} dur={c - b} />
      </Sequence>
      <Sequence from={c} durationInFrames={d - c} name="resilient">
        <ShotResilient L={L} base={c} dur={d - c} />
      </Sequence>
      <Sequence from={d} durationInFrames={s.duration - d} name="endcard">
        <EndCard L={L} base={d} />
      </Sequence>
    </AbsoluteFill>
  );
};
