import React, {createContext, useContext} from 'react';
import tl from '../data/timeline.json';

export type LineT = {id: string; from: number; duration: number; text: string; source?: string | null; pauses: number[]};
export type SceneT = {
  id: string;
  chapter: string | null;
  clock: number | null;
  from: number;
  duration: number;
  lines: LineT[];
};
export type TimelineT = {fps: number; width: number; height: number; durationInFrames: number; scenes: SceneT[]};

export const TIMELINE = tl as TimelineT;
export const getScene = (id: string) => {
  const s = TIMELINE.scenes.find((x) => x.id === id);
  if (!s) throw new Error('Unknown scene ' + id);
  return s;
};

export const SceneCtx = createContext<SceneT | null>(null);
export const useScene = () => {
  const s = useContext(SceneCtx);
  if (!s) throw new Error('useScene outside SceneCtx');
  return s;
};

/** Map of line id -> {from, end, duration} in scene-local frames. */
export const useLines = () => {
  const s = useScene();
  const m: Record<string, {from: number; end: number; duration: number; p: number[]}> = {};
  // p = absolute (scene-local) frames of natural pauses inside the line, for word-level sync
  for (const l of s.lines) m[l.id] = {from: l.from, end: l.from + l.duration, duration: l.duration, p: l.pauses.map((x) => l.from + x)};
  return m;
};

export const SceneProvider: React.FC<{scene: SceneT; children: React.ReactNode}> = ({scene, children}) =>
  React.createElement(SceneCtx.Provider, {value: scene}, children);
