import React from 'react';
import {AbsoluteFill, useCurrentFrame, useVideoConfig} from 'remotion';
import {Stage} from '../components/three/Stage';
import {Globe, Stars} from '../components/three/Globe';
import {Logo} from '../components/ui/Logo';
import {easeOut, pop, prog} from '../lib/anim';
import {C, FONT_HEAD} from '../theme';
import {Sfx} from '../audio/Sfx';

export const Title: React.FC = () => {
  const f = useCurrentFrame();
  const {fps, durationInFrames} = useVideoConfig();
    const out = prog(f, 120, 135);
  return (
    <AbsoluteFill>
      <Stage bg="#03050f" drift={0.02} cam={[{f: 0, pos: [0, -1.2, 5.4], target: [0, 0.8, 0], fov: 40}, {f: 135, pos: [0, -1.4, 4.6], target: [0, 0.9, 0], fov: 40}]}>
        <ambientLight intensity={0.2} />
        <directionalLight position={[-4, 2, 5]} intensity={1.0} color="#ffc9d3" />
        <Stars count={1200} opacity={0.7} />
        <Globe frame={f} rotation={-3.4 - f * 0.002} connectivity={0} landGlow={0.25} tilt={0.5} />
      </Stage>
      <AbsoluteFill style={{background: 'radial-gradient(ellipse at 50% 40%, rgba(255,77,109,0.18), rgba(0,0,0,0) 60%)', opacity: prog(f, 0, 20)}} />
      <AbsoluteFill style={{alignItems: 'center', justifyContent: 'center', opacity: 1 - out, transform: `scale(${1 + f * 0.0006})`}}>
        <div style={{transform: `translateY(${(1 - prog(f, 0, 18, easeOut)) * -20}px)`, opacity: prog(f, 0, 14), marginBottom: 40}}>
          <Logo size={1.1} />
        </div>
        {[
          ['What', 'If', 'the', 'Entire', 'Internet'],
          ['Shut', 'Down', 'for', '24 Hours?'],
        ].map((row, r) => (
          <div key={r} style={{display: 'flex', gap: 30, justifyContent: 'center'}}>
            {row.map((w, i) => {
              const s = pop(f, fps, 10 + (r * 5 + i) * 3, 14);
              const hot = w.startsWith('24');
              return (
                <span
                  key={i}
                  style={{
                    fontFamily: FONT_HEAD,
                    fontWeight: 700,
                    fontSize: 118,
                    lineHeight: 1.12,
                    color: hot ? C.alert : C.white,
                    opacity: Math.min(1, s),
                    transform: `translateY(${(1 - s) * 40}px) scale(${hot ? 0.9 + 0.1 * s : 1})`,
                    display: 'inline-block',
                    textShadow: hot ? `0 0 50px ${C.alert}88` : '0 8px 40px rgba(0,0,0,0.7)',
                  }}
                >
                  {w}
                </span>
              );
            })}
          </div>
        ))}
      </AbsoluteFill>
      <AbsoluteFill style={{background: '#fff', opacity: 0.22 * (1 - prog(f, 0, 8))}} />
      <Sfx name="hit" at={0} volume={0.8} />
      <Sfx name="whooshShort" at={34} volume={0.4} />
    </AbsoluteFill>
  );
};
