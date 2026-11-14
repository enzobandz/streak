import React from 'react';
import {HAIRS, SHIRTS, SKINS} from '../../theme';
import {rnd} from '../../lib/anim';

export type PersonLook = {shirt: string; skin: string; hair: string; pants: string; hairStyle: 0 | 1 | 2};

/** Deterministic outfit from a seed so crowds look varied but stable between renders. */
export const lookFor = (seed: string | number): PersonLook => ({
  shirt: SHIRTS[Math.floor(rnd('sh' + seed) * SHIRTS.length)],
  skin: SKINS[Math.floor(rnd('sk' + seed) * SKINS.length)],
  hair: HAIRS[Math.floor(rnd('hr' + seed) * HAIRS.length)],
  pants: ['#2E3A59', '#3D3D5C', '#4A4E69', '#1F2A44', '#5C4B51'][Math.floor(rnd('pa' + seed) * 5)],
  hairStyle: Math.floor(rnd('hs' + seed) * 3) as 0 | 1 | 2,
});

export type Pose = {
  walk?: number; // walk-cycle phase (radians); amplitude via walkAmp
  walkAmp?: number;
  phone?: number; // 0..1 raise phone to look at it
  phoneScreen?: string; // emissive color of phone screen
  armL?: number; // extra shoulder rotation (x axis) for left arm
  armR?: number;
  armLz?: number; // sideways raise
  armRz?: number;
  headYaw?: number;
  headPitch?: number;
  headTilt?: number;
  mood?: number; // 1 smile, 0 neutral, -1 worried
  blink?: boolean;
  bob?: number;
  shrug?: number; // 0..1
  hold?: 'phone' | 'clipboard' | 'cash' | 'papers' | 'none';
};

const Capsule: React.FC<{r: number; len: number; color: string; position?: [number, number, number]}> = ({r, len, color, position}) => (
  <mesh position={position} castShadow>
    <capsuleGeometry args={[r, len, 6, 12]} />
    <meshStandardMaterial color={color} roughness={0.6} />
  </mesh>
);

export const Person: React.FC<{
  look: PersonLook;
  pose?: Pose;
  position?: [number, number, number];
  rotation?: number; // y rotation
  scale?: number;
}> = ({look, pose = {}, position = [0, 0, 0], rotation = 0, scale = 1}) => {
  const {
    walk = 0,
    walkAmp = 0,
    phone = 0,
    phoneScreen = '#7fe7ff',
    armL = 0,
    armR = 0,
    armLz = 0,
    armRz = 0,
    headYaw = 0,
    headPitch = 0,
    headTilt = 0,
    mood = 0.6,
    blink = false,
    bob = 0,
    shrug = 0,
    hold = phone > 0 ? 'phone' : 'none',
  } = pose;
  const swing = Math.sin(walk) * walkAmp;
  const lift = Math.abs(Math.cos(walk)) * walkAmp * 0.06 + bob;
  const sh = shrug * 0.08;
  // right arm brings phone in front of face
  const rArmX = -swing * 0.8 * (1 - phone) - phone * 1.05 + armR;
  const lArmX = swing * 0.8 + armL;
  return (
    <group position={position} rotation={[0, rotation, 0]} scale={scale}>
      <group position={[0, lift, 0]}>
        {/* legs */}
        {[-1, 1].map((side) => (
          <group key={side} position={[side * 0.11, 0.78, 0]} rotation={[side * swing * 0.9, 0, 0]}>
            <Capsule r={0.095} len={0.5} color={look.pants} position={[0, -0.36, 0]} />
            <mesh position={[0, -0.72, 0.05]} castShadow>
              <boxGeometry args={[0.16, 0.09, 0.26]} />
              <meshStandardMaterial color="#20232f" roughness={0.7} />
            </mesh>
          </group>
        ))}
        {/* torso */}
        <Capsule r={0.24} len={0.42} color={look.shirt} position={[0, 1.1, 0]} />
        {/* arms */}
        <group position={[-0.31, 1.3 + sh, 0]} rotation={[lArmX, 0, -0.12 - armLz - shrug * 0.3]}>
          <Capsule r={0.07} len={0.42} color={look.shirt} position={[0, -0.24, 0]} />
          <mesh position={[0, -0.5, 0]} castShadow>
            <sphereGeometry args={[0.075, 12, 10]} />
            <meshStandardMaterial color={look.skin} roughness={0.6} />
          </mesh>
          {hold === 'clipboard' ? (
            <mesh position={[0.05, -0.55, 0.12]} rotation={[0.3, 0, 0]}>
              <boxGeometry args={[0.26, 0.34, 0.02]} />
              <meshStandardMaterial color="#f4f1e6" />
            </mesh>
          ) : null}
        </group>
        <group position={[0.31, 1.3 + sh, 0]} rotation={[rArmX, 0, 0.12 + armRz + shrug * 0.3 - phone * 0.32]}>
          <Capsule r={0.07} len={0.42} color={look.shirt} position={[0, -0.24, 0]} />
          <mesh position={[0, -0.5, 0]} castShadow>
            <sphereGeometry args={[0.075, 12, 10]} />
            <meshStandardMaterial color={look.skin} roughness={0.6} />
          </mesh>
          {hold === 'phone' ? (
            <group position={[0, -0.56, 0.04]} rotation={[-0.35 + phone * 1.2, 0, 0]}>
              <mesh>
                <boxGeometry args={[0.1, 0.19, 0.018]} />
                <meshStandardMaterial color="#1b1e2b" roughness={0.3} />
              </mesh>
              <mesh position={[0, 0, 0.011]}>
                <planeGeometry args={[0.085, 0.17]} />
                <meshBasicMaterial color={phoneScreen} toneMapped={false} />
              </mesh>
            </group>
          ) : null}
          {hold === 'cash' ? (
            <mesh position={[0, -0.6, 0.06]} rotation={[0.2, 0.2, 0]}>
              <boxGeometry args={[0.22, 0.11, 0.015]} />
              <meshStandardMaterial color="#7cc98a" />
            </mesh>
          ) : null}
          {hold === 'papers' ? (
            <mesh position={[0, -0.58, 0.1]} rotation={[0.5, 0, 0]}>
              <boxGeometry args={[0.24, 0.3, 0.04]} />
              <meshStandardMaterial color="#fbfaf5" />
            </mesh>
          ) : null}
        </group>
        {/* head */}
        <group position={[0, 1.6 + sh * 0.3, 0]} rotation={[headPitch + phone * 0.35, headYaw, headTilt]}>
          <mesh castShadow>
            <sphereGeometry args={[0.24, 24, 18]} />
            <meshStandardMaterial color={look.skin} roughness={0.55} />
          </mesh>
          {/* hair */}
          {look.hairStyle === 0 ? (
            <mesh position={[0, 0.05, -0.02]} rotation={[-0.25, 0, 0]}>
              <sphereGeometry args={[0.255, 24, 16, 0, Math.PI * 2, 0, Math.PI * 0.5]} />
              <meshStandardMaterial color={look.hair} roughness={0.8} />
            </mesh>
          ) : look.hairStyle === 1 ? (
            <>
              <mesh position={[0, 0.04, -0.03]} rotation={[-0.4, 0, 0]}>
                <sphereGeometry args={[0.26, 24, 16, 0, Math.PI * 2, 0, Math.PI * 0.62]} />
                <meshStandardMaterial color={look.hair} roughness={0.8} />
              </mesh>
              <mesh position={[0, -0.05, -0.16]}>
                <sphereGeometry args={[0.15, 16, 12]} />
                <meshStandardMaterial color={look.hair} roughness={0.8} />
              </mesh>
            </>
          ) : (
            <mesh position={[0, 0.17, -0.02]}>
              <sphereGeometry args={[0.2, 16, 12]} />
              <meshStandardMaterial color={look.hair} roughness={0.9} />
            </mesh>
          )}
          {/* eyes */}
          {[-1, 1].map((side) => (
            <mesh key={side} position={[side * 0.085, 0.02, 0.215]} scale={[1, blink ? 0.15 : 1, 1]}>
              <sphereGeometry args={[0.03, 12, 10]} />
              <meshBasicMaterial color="#141522" />
            </mesh>
          ))}
          {/* brows for worried look */}
          {mood < 0
            ? [-1, 1].map((side) => (
                <mesh key={side} position={[side * 0.085, 0.09, 0.212]} rotation={[0, 0, side * 0.35 * -mood]}>
                  <boxGeometry args={[0.07, 0.014, 0.01]} />
                  <meshBasicMaterial color="#141522" />
                </mesh>
              ))
            : null}
          {/* mouth */}
          {Math.abs(mood) < 0.2 ? (
            <mesh position={[0, -0.08, 0.222]}>
              <boxGeometry args={[0.07, 0.014, 0.01]} />
              <meshBasicMaterial color="#5b2a2a" />
            </mesh>
          ) : (
            <mesh position={[0, mood > 0 ? -0.06 : -0.1, 0.214]} rotation={[0, 0, mood > 0 ? Math.PI : 0]}>
              <torusGeometry args={[0.05, 0.012, 6, 16, Math.PI]} />
              <meshBasicMaterial color="#5b2a2a" />
            </mesh>
          )}
          {/* cheeks */}
          {mood > 0.4
            ? [-1, 1].map((side) => (
                <mesh key={side} position={[side * 0.14, -0.05, 0.18]}>
                  <circleGeometry args={[0.035, 12]} />
                  <meshBasicMaterial color="#ff8a8a" transparent opacity={0.35} />
                </mesh>
              ))
            : null}
        </group>
      </group>
    </group>
  );
};

/** Blink helper: deterministic blinks every few seconds per character. */
export const isBlink = (frame: number, seed: string | number) => {
  const period = 90 + Math.floor(rnd('bl' + seed) * 80);
  const off = Math.floor(rnd('bo' + seed) * period);
  return (frame + off) % period < 4;
};

export {SHIRTS, SKINS, HAIRS};
