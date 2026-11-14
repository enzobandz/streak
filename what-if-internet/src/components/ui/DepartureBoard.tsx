import React from 'react';
import {useCurrentFrame} from 'remotion';
import {C, FONT_HEAD} from '../../theme';
import {prog} from '../../lib/anim';

export type Flight = {time: string; dest: string; code: string; status: 'ON TIME' | 'DELAYED' | 'CANCELED' | 'LANDED'};

const statusColor = (s: Flight['status']) => (s === 'ON TIME' || s === 'LANDED' ? C.ok : s === 'DELAYED' ? C.amber : C.alert);

/** Split-flap style departures board. Rows flip to their new status at flipAt + i*stagger. */
export const DepartureBoard: React.FC<{flights: Flight[]; after: Flight['status'][]; flipAt: number; x?: number; y?: number; opacity?: number}> = ({
  flights,
  after,
  flipAt,
  x = 160,
  y = 200,
  opacity = 1,
}) => {
  const f = useCurrentFrame();
  return (
    <div style={{position: 'absolute', left: x, top: y, width: 1000, background: '#0b0f1e', borderRadius: 20, padding: 26, border: '3px solid #262c48', boxShadow: '0 30px 80px rgba(0,0,0,0.6)', opacity}}>
      <div style={{display: 'flex', fontFamily: FONT_HEAD, fontWeight: 700, color: C.amber, fontSize: 30, letterSpacing: 4, marginBottom: 14}}>
        <div style={{width: 150}}>TIME</div>
        <div style={{flex: 1}}>DESTINATION</div>
        <div style={{width: 160}}>FLIGHT</div>
        <div style={{width: 240}}>STATUS</div>
      </div>
      {flights.map((fl, i) => {
        const t = flipAt + i * 7;
        const flipping = f >= t && f < t + 8;
        const st = f >= t + 4 ? after[i] : fl.status;
        return (
          <div key={i} style={{display: 'flex', fontFamily: FONT_HEAD, fontWeight: 500, color: '#f0f2f8', fontSize: 38, padding: '9px 0', borderTop: '2px solid #1b2038'}}>
            <div style={{width: 150}}>{fl.time}</div>
            <div style={{flex: 1}}>{fl.dest}</div>
            <div style={{width: 160, color: C.mist}}>{fl.code}</div>
            <div style={{width: 240, color: statusColor(st), transform: flipping ? `scaleY(${Math.abs(Math.cos(((f - t) / 8) * Math.PI))})` : undefined, opacity: st === 'CANCELED' ? 0.75 + 0.25 * Math.round(prog(f % 30, 0, 15)) : 1}}>
              {st}
            </div>
          </div>
        );
      })}
    </div>
  );
};
