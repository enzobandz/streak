import React from 'react';
import {useCurrentFrame} from 'remotion';
import {C, FONT_HEAD} from '../../theme';

/** "What If Reality" channel mark: an orbit ring with a travelling node + wordmark. */
export const Logo: React.FC<{size?: number; opacity?: number}> = ({size = 1, opacity = 1}) => {
  const f = useCurrentFrame();
  const a = f * 0.06;
  return (
    <div style={{display: 'flex', alignItems: 'center', gap: 22 * size, opacity}}>
      <svg width={84 * size} height={84 * size} viewBox="0 0 84 84">
        <circle cx={42} cy={42} r={30} stroke={C.data} strokeWidth={4} fill="none" opacity={0.9} />
        <ellipse cx={42} cy={42} rx={38} ry={14} stroke={C.white} strokeWidth={2} fill="none" opacity={0.45} transform="rotate(-24 42 42)" />
        <text x={42} y={54} textAnchor="middle" fontFamily={FONT_HEAD} fontWeight={700} fontSize={36} fill={C.white}>?</text>
        <circle cx={42 + Math.cos(a) * 38 * 0.914 - Math.sin(a) * 14 * 0.407} cy={42 + Math.cos(a) * 38 * -0.407 + Math.sin(a) * 14 * 0.914} r={5} fill={C.amber} />
      </svg>
      <div style={{fontFamily: FONT_HEAD, lineHeight: 0.92}}>
        <div style={{fontWeight: 700, fontSize: 40 * size, letterSpacing: 6 * size, color: C.white}}>WHAT IF</div>
        <div style={{fontWeight: 500, fontSize: 40 * size, letterSpacing: 6 * size, color: C.data}}>REALITY</div>
      </div>
    </div>
  );
};
