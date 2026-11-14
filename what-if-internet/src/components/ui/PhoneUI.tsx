import React from 'react';
import {useCurrentFrame} from 'remotion';
import {C, FONT_BODY} from '../../theme';
import {prog} from '../../lib/anim';
import {Icon} from './Icons';

/** 2D phone mock-up showing a map: GPS dot works, tiles may or may not load. */
export const PhoneMap: React.FC<{x: number; y: number; tilesLoaded: number; offline?: number; opacity?: number}> = ({x, y, tilesLoaded, offline = 0, opacity = 1}) => {
  const f = useCurrentFrame();
  const pulse = (f % 40) / 40;
  return (
    <div style={{position: 'absolute', left: x, top: y, width: 380, height: 760, borderRadius: 56, background: '#10131f', padding: 16, boxShadow: '0 30px 80px rgba(0,0,0,0.6)', opacity}}>
      <div style={{position: 'relative', width: '100%', height: '100%', borderRadius: 42, overflow: 'hidden', background: '#d9dde8'}}>
        <div style={{position: 'absolute', inset: 0, display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gridTemplateRows: 'repeat(8, 1fr)'}}>
          {Array.from({length: 32}).map((_, i) => {
            const loaded = Math.max(tilesLoaded, offline) > 0.5 || (i * 7) % 11 === 0 ? 1 : 0;
            return (
              <div key={i} style={{border: '1px solid #c3c8d6', background: loaded ? (i % 5 === 0 ? '#bfe3c2' : i % 7 === 0 ? '#a9d4f5' : '#f2efe6') : '#cfd3de', position: 'relative'}}>
                {loaded ? null : <div style={{position: 'absolute', inset: 0, background: 'repeating-linear-gradient(45deg, #c7ccd8 0 6px, #d4d8e2 6px 12px)'}} />}
                {loaded && i % 3 === 1 ? <div style={{position: 'absolute', left: 0, right: 0, top: '45%', height: 6, background: '#ffffff'}} /> : null}
                {loaded && i % 4 === 2 ? <div style={{position: 'absolute', top: 0, bottom: 0, left: '40%', width: 6, background: '#ffffff'}} /> : null}
              </div>
            );
          })}
        </div>
        <div style={{position: 'absolute', left: '50%', top: '52%', transform: 'translate(-50%,-50%)'}}>
          <div style={{position: 'absolute', left: -40 * pulse, top: -40 * pulse, width: 26 + 80 * pulse, height: 26 + 80 * pulse, marginLeft: -13 + 0, marginTop: -13, borderRadius: '50%', background: '#2f80ff', opacity: 0.35 * (1 - pulse)}} />
          <div style={{width: 26, height: 26, borderRadius: 13, background: '#2f80ff', border: '4px solid white', marginLeft: -13, marginTop: -13, boxShadow: '0 2px 8px rgba(0,0,0,0.3)'}} />
        </div>
        <div style={{position: 'absolute', left: 14, right: 14, top: 40, padding: '12px 16px', borderRadius: 18, background: 'white', fontFamily: FONT_BODY, fontWeight: 600, fontSize: 22, color: C.ink, display: 'flex', alignItems: 'center', gap: 10, boxShadow: '0 4px 14px rgba(0,0,0,0.15)'}}>
          <Icon name={offline > 0.5 ? 'check' : 'wifiOff'} size={28} color={offline > 0.5 ? '#1aa865' : C.alert} stroke={2.6} />
          {offline > 0.5 ? 'Offline map saved' : 'Map tiles unavailable'}
        </div>
        <div style={{position: 'absolute', left: 14, right: 14, bottom: 26, padding: '12px 16px', borderRadius: 18, background: '#1d2236', color: 'white', fontFamily: FONT_BODY, fontSize: 21, display: 'flex', alignItems: 'center', gap: 10, opacity: prog(f, 10, 20)}}>
          <Icon name="satellite" size={28} color={C.ok} />
          GPS position: OK
        </div>
      </div>
    </div>
  );
};
