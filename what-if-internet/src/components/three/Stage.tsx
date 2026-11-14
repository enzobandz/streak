import React from 'react';
import {ThreeCanvas} from '@remotion/three';
import {useCurrentFrame, useVideoConfig} from 'remotion';
import {useThree} from '@react-three/fiber';
import * as THREE from 'three';
import {keyVec} from '../../lib/anim';

export type CamKey = {f: number; pos: number[]; target: number[]; fov?: number};

/** Deterministic keyframed camera with optional subtle handheld drift. */
export const CameraRig: React.FC<{keys: CamKey[]; drift?: number}> = ({keys, drift = 0.02}) => {
  const f = useCurrentFrame();
  const camera = useThree((s) => s.camera) as THREE.PerspectiveCamera;
  const pos = keyVec(f, keys.map((k) => [k.f, k.pos]));
  const tgt = keyVec(f, keys.map((k) => [k.f, k.target]));
  const fov = keyVec(f, keys.map((k) => [k.f, [k.fov ?? 40]]))[0];
  const d = drift;
  camera.position.set(
    pos[0] + Math.sin(f * 0.031) * d + Math.sin(f * 0.013) * d,
    pos[1] + Math.sin(f * 0.023 + 1) * d,
    pos[2] + Math.cos(f * 0.017) * d,
  );
  camera.lookAt(tgt[0], tgt[1], tgt[2]);
  if (camera.fov !== fov) {
    camera.fov = fov;
    camera.updateProjectionMatrix();
  }
  return null;
};

export const Stage: React.FC<{
  children: React.ReactNode;
  cam: CamKey[];
  bg?: string;
  fog?: [string, number, number];
  shadows?: boolean;
  drift?: number;
  style?: React.CSSProperties;
  width?: number;
  height?: number;
}> = ({children, cam, bg = '#0E1636', fog, shadows = false, drift, style, width: w, height: h}) => {
  const vc = useVideoConfig();
  const width = w ?? vc.width;
  const height = h ?? vc.height;
  return (
    <ThreeCanvas
      width={width}
      height={height}
      shadows={shadows}
      dpr={1}
      gl={{antialias: true, preserveDrawingBuffer: true}}
      camera={{fov: cam[0].fov ?? 40, near: 0.1, far: 400, position: cam[0].pos as [number, number, number]}}
      style={{position: 'absolute', inset: 0, ...style}}
    >
      <color attach="background" args={[bg]} />
      {fog ? <fog attach="fog" args={fog} /> : null}
      <CameraRig keys={cam} drift={drift} />
      {children}
    </ThreeCanvas>
  );
};

/** Standard three-point-ish lighting rig used across scenes for a consistent look. */
export const Lights: React.FC<{
  sun?: string;
  sky?: string;
  ground?: string;
  intensity?: number;
  sunPos?: [number, number, number];
  shadow?: boolean;
  shadowSize?: number;
}> = ({sun = '#fff1dc', sky = '#bcd7ff', ground = '#3a3550', intensity = 1, sunPos = [8, 14, 6], shadow = false, shadowSize = 18}) => (
  <>
    <hemisphereLight args={[sky, ground, 0.9 * intensity]} />
    <directionalLight
      position={sunPos}
      intensity={2.2 * intensity}
      color={sun}
      castShadow={shadow}
      shadow-mapSize-width={2048}
      shadow-mapSize-height={2048}
      shadow-camera-left={-shadowSize}
      shadow-camera-right={shadowSize}
      shadow-camera-top={shadowSize}
      shadow-camera-bottom={-shadowSize}
      shadow-camera-far={80}
      shadow-bias={-0.0004}
    />
    <directionalLight position={[-6, 4, -8]} intensity={0.6 * intensity} color="#8fb4ff" />
  </>
);
