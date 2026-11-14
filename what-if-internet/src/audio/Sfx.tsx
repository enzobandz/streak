import React from 'react';
import {Audio, Sequence, staticFile} from 'remotion';

export type SfxName =
  | 'whoosh' | 'whooshShort' | 'boom' | 'hit' | 'riser' | 'powerDown' | 'powerUp' | 'glitch' | 'blip' | 'pop'
  | 'tick' | 'error' | 'chime' | 'ping' | 'coin' | 'switch' | 'static' | 'radar' | 'jet' | 'city' | 'typing'
  | 'hum' | 'pulse';

/** Place a sound effect at a (sequence-local) frame. */
export const Sfx: React.FC<{name: SfxName; at: number; volume?: number; len?: number}> = ({name, at, volume = 0.6, len = 300}) => (
  <Sequence from={Math.round(at)} durationInFrames={len} layout="none" name={`sfx:${name}`}>
    <Audio src={staticFile(`sfx/${name}.wav`)} volume={volume} />
  </Sequence>
);
