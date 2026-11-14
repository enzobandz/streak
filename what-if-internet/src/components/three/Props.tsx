import React, {useMemo} from 'react';
import * as THREE from 'three';
import {C} from '../../theme';
import {rnd} from '../../lib/anim';

type V3 = [number, number, number];
const Std: React.FC<{color: string; rough?: number; metal?: number; emissive?: string; ei?: number; flat?: boolean}> = ({color, rough = 0.6, metal = 0, emissive, ei = 0, flat}) => (
  <meshStandardMaterial color={color} roughness={rough} metalness={metal} emissive={emissive ?? '#000'} emissiveIntensity={ei} flatShading={flat} />
);
const Glow: React.FC<{color: string; k?: number; opacity?: number}> = ({color, k = 1, opacity = 1}) => (
  <meshBasicMaterial color={new THREE.Color(color).multiplyScalar(k)} toneMapped={false} transparent={opacity < 1} opacity={opacity} />
);

/* Home Wi-Fi router with status LEDs */
export const Router: React.FC<{position?: V3; leds: string[]; scale?: number}> = ({position = [0, 0, 0], leds, scale = 1}) => (
  <group position={position} scale={scale}>
    <mesh position={[0, 0.12, 0]} castShadow>
      <boxGeometry args={[1.2, 0.24, 0.8]} />
      <Std color="#f1f3f9" rough={0.35} />
    </mesh>
    {[-0.45, 0.45].map((x) => (
      <mesh key={x} position={[x, 0.6, -0.32]} rotation={[0, 0, x * 0.2]} castShadow>
        <cylinderGeometry args={[0.035, 0.035, 0.9, 8]} />
        <Std color="#e3e6ef" />
      </mesh>
    ))}
    {leds.map((c, i) => (
      <mesh key={i} position={[-0.36 + i * 0.18, 0.13, 0.405]}>
        <circleGeometry args={[0.035, 12]} />
        <Glow color={c} k={1.4} />
      </mesh>
    ))}
  </group>
);

/* Card payment terminal; screen colour driven by scene */
export const CardTerminal: React.FC<{position?: V3; screen: string; rotation?: number}> = ({position = [0, 0, 0], screen, rotation = 0}) => (
  <group position={position} rotation={[0, rotation, 0]}>
    <mesh position={[0, 0.25, 0]} rotation={[-0.35, 0, 0]} castShadow>
      <boxGeometry args={[0.42, 0.62, 0.12]} />
      <Std color="#2a2f45" rough={0.4} />
    </mesh>
    <mesh position={[0, 0.36, 0.08]} rotation={[-0.35, 0, 0]}>
      <planeGeometry args={[0.32, 0.22]} />
      <Glow color={screen} />
    </mesh>
    {Array.from({length: 9}).map((_, i) => (
      <mesh key={i} position={[-0.1 + (i % 3) * 0.1, 0.17 - Math.floor(i / 3) * 0.075 * 0.94, 0.075 - Math.floor(i / 3) * 0.026]} rotation={[-0.35, 0, 0]}>
        <boxGeometry args={[0.07, 0.05, 0.02]} />
        <Std color="#d6dae8" />
      </mesh>
    ))}
  </group>
);

export const ATM: React.FC<{position?: V3; screen: string; rotation?: number}> = ({position = [0, 0, 0], screen, rotation = 0}) => (
  <group position={position} rotation={[0, rotation, 0]}>
    <mesh position={[0, 1.0, 0]} castShadow>
      <boxGeometry args={[1.1, 2.0, 0.8]} />
      <Std color="#3d5a9e" rough={0.4} metal={0.2} />
    </mesh>
    <mesh position={[0, 1.95, 0.05]} castShadow>
      <boxGeometry args={[1.2, 0.3, 0.95]} />
      <Std color="#2c447d" />
    </mesh>
    <mesh position={[0, 1.35, 0.41]}>
      <planeGeometry args={[0.7, 0.45]} />
      <Glow color={screen} />
    </mesh>
    <mesh position={[0, 0.9, 0.42]}>
      <boxGeometry args={[0.5, 0.06, 0.04]} />
      <Std color="#111" />
    </mesh>
    <mesh position={[0, 0.7, 0.42]}>
      <boxGeometry args={[0.6, 0.18, 0.04]} />
      <Std color="#c9d1e6" />
    </mesh>
  </group>
);

/* Lattice power pylon */
export const Pylon: React.FC<{position?: V3; scale?: number}> = ({position = [0, 0, 0], scale = 1}) => (
  <group position={position} scale={scale}>
    {[
      [-0.6, -0.6],
      [0.6, -0.6],
      [-0.6, 0.6],
      [0.6, 0.6],
    ].map(([x, z], i) => (
      <mesh key={i} position={[x / 2, 3, z / 2]} rotation={[z * -0.1, 0, x * 0.1]}>
        <cylinderGeometry args={[0.05, 0.08, 6, 5]} />
        <Std color="#8d97b5" metal={0.5} rough={0.4} />
      </mesh>
    ))}
    {[4.4, 5.4].map((y) => (
      <mesh key={y} position={[0, y, 0]}>
        <boxGeometry args={[y > 5 ? 2.2 : 3, 0.1, 0.12]} />
        <Std color="#8d97b5" metal={0.5} rough={0.4} />
      </mesh>
    ))}
    <mesh position={[0, 6.2, 0]}>
      <coneGeometry args={[0.25, 1.2, 4]} />
      <Std color="#8d97b5" metal={0.5} />
    </mesh>
  </group>
);

/* Radio / TV broadcast mast with emitted rings */
export const BroadcastTower: React.FC<{position?: V3; frame: number; on?: number; color?: string; scale?: number}> = ({position = [0, 0, 0], frame, on = 1, color = C.ok, scale = 1}) => (
  <group position={position} scale={scale}>
    <mesh position={[0, 4, 0]}>
      <coneGeometry args={[0.8, 8, 4, 1, true]} />
      <meshStandardMaterial color="#d8434f" wireframe />
    </mesh>
    <mesh position={[0, 4, 0]}>
      <cylinderGeometry args={[0.05, 0.12, 8, 6]} />
      <Std color="#e9edf7" />
    </mesh>
    <mesh position={[0, 8.15, 0]}>
      <sphereGeometry args={[0.18, 12, 10]} />
      <Glow color="#ff5a5a" k={on * 1.5 + 0.2} />
    </mesh>
    {[0, 1, 2, 3].map((k) => {
      const t = ((frame * 0.012 + k / 4) % 1) * on;
      return (
        <mesh key={k} position={[0, 8.1, 0]} rotation={[Math.PI / 2, 0, 0]} scale={0.5 + t * 9}>
          <torusGeometry args={[1, 0.02, 6, 48]} />
          <Glow color={color} opacity={(1 - t) * 0.8 * on} />
        </mesh>
      );
    })}
  </group>
);

/* Server rack with blinking LEDs (deterministic) */
export const ServerRack: React.FC<{position?: V3; frame: number; power?: number; seed?: string; ledColor?: string}> = ({position = [0, 0, 0], frame, power = 1, seed = 'r', ledColor = C.data}) => (
  <group position={position}>
    <mesh position={[0, 1.1, 0]} castShadow receiveShadow>
      <boxGeometry args={[0.9, 2.2, 1.1]} />
      <Std color="#1d2236" rough={0.4} metal={0.3} />
    </mesh>
    {Array.from({length: 10}).map((_, i) => (
      <group key={i} position={[0, 0.25 + i * 0.19, 0.56]}>
        <mesh>
          <planeGeometry args={[0.8, 0.15]} />
          <Std color="#2c3352" />
        </mesh>
        {[0, 1, 2].map((k) => {
          const blink = rnd(`${seed}${i}${k}${Math.floor(frame / (3 + k * 2))}`) > 0.35 ? 1 : 0.15;
          return (
            <mesh key={k} position={[0.25 + k * 0.06, 0, 0.005]}>
              <circleGeometry args={[0.018, 8]} />
              <Glow color={k === 2 ? C.ok : ledColor} k={(0.1 + blink * 1.3) * power} />
            </mesh>
          );
        })}
      </group>
    ))}
  </group>
);

/* Desk with monitor (screen colour from scene) */
export const Desk: React.FC<{position?: V3; rotation?: number; screen: string}> = ({position = [0, 0, 0], rotation = 0, screen}) => (
  <group position={position} rotation={[0, rotation, 0]}>
    <mesh position={[0, 0.74, 0]} castShadow receiveShadow>
      <boxGeometry args={[1.6, 0.06, 0.8]} />
      <Std color="#e8d6b9" />
    </mesh>
    {[-0.7, 0.7].map((x) => (
      <mesh key={x} position={[x, 0.37, 0]}>
        <boxGeometry args={[0.06, 0.74, 0.7]} />
        <Std color="#9aa1b8" />
      </mesh>
    ))}
    <mesh position={[0, 1.15, -0.2]} castShadow>
      <boxGeometry args={[0.9, 0.56, 0.05]} />
      <Std color="#20243a" />
    </mesh>
    <mesh position={[0, 1.15, -0.172]}>
      <planeGeometry args={[0.82, 0.48]} />
      <Glow color={screen} />
    </mesh>
    <mesh position={[0, 0.85, -0.2]}>
      <boxGeometry args={[0.08, 0.2, 0.06]} />
      <Std color="#20243a" />
    </mesh>
  </group>
);

/* Industrial robot arm, angles driven by frame */
export const RobotArm: React.FC<{position?: V3; a: number; b: number; c: number; color?: string}> = ({position = [0, 0, 0], a, b, c, color = '#ffb547'}) => (
  <group position={position}>
    <mesh position={[0, 0.2, 0]} castShadow>
      <cylinderGeometry args={[0.5, 0.6, 0.4, 16]} />
      <Std color="#3b4157" />
    </mesh>
    <group position={[0, 0.4, 0]} rotation={[0, a, 0]}>
      <group rotation={[0, 0, b]}>
        <mesh position={[0, 0.8, 0]} castShadow>
          <boxGeometry args={[0.32, 1.6, 0.32]} />
          <Std color={color} rough={0.4} />
        </mesh>
        <group position={[0, 1.6, 0]} rotation={[0, 0, c]}>
          <mesh position={[0.6, 0, 0]} castShadow>
            <boxGeometry args={[1.2, 0.24, 0.24]} />
            <Std color={color} rough={0.4} />
          </mesh>
          <mesh position={[1.25, -0.1, 0]}>
            <boxGeometry args={[0.16, 0.3, 0.3]} />
            <Std color="#3b4157" />
          </mesh>
        </group>
      </group>
    </group>
  </group>
);

/* Cargo box */
export const Box: React.FC<{position: V3; color?: string; s?: number}> = ({position, color = '#c89b6a', s = 0.5}) => (
  <mesh position={position} castShadow>
    <boxGeometry args={[s, s * 0.8, s]} />
    <Std color={color} rough={0.9} />
  </mesh>
);

/* Stylised airliner */
export const Plane: React.FC<{position?: V3; rotation?: V3; scale?: number; livery?: string}> = ({position = [0, 0, 0], rotation = [0, 0, 0], scale = 1, livery = C.data}) => (
  <group position={position} rotation={rotation} scale={scale}>
    <mesh rotation={[0, 0, Math.PI / 2]} castShadow>
      <capsuleGeometry args={[0.35, 3.2, 8, 16]} />
      <Std color="#f4f6fb" rough={0.3} />
    </mesh>
    <mesh position={[0.1, -0.05, 0]} castShadow>
      <boxGeometry args={[0.8, 0.06, 4.2]} />
      <Std color="#e2e6f0" />
    </mesh>
    <mesh position={[-1.75, 0.55, 0]} castShadow>
      <boxGeometry args={[0.6, 0.9, 0.06]} />
      <Std color={livery} />
    </mesh>
    <mesh position={[-1.75, 0.1, 0]}>
      <boxGeometry args={[0.45, 0.05, 1.4]} />
      <Std color="#e2e6f0" />
    </mesh>
    {[-1, 1].map((s) => (
      <mesh key={s} position={[0.25, -0.25, s * 0.95]} rotation={[0, 0, Math.PI / 2]}>
        <cylinderGeometry args={[0.15, 0.15, 0.6, 12]} />
        <Std color="#b9c0d3" metal={0.4} />
      </mesh>
    ))}
    <mesh position={[0.4, 0.05, 0]} rotation={[0, 0, Math.PI / 2]}>
      <cylinderGeometry args={[0.352, 0.352, 2.4, 16, 1, true]} />
      <meshStandardMaterial color={livery} transparent opacity={0.0} />
    </mesh>
    <mesh position={[0.2, 0.0, 0]} scale={[2.2, 0.18, 1.02]}>
      <sphereGeometry args={[0.35, 16, 8]} />
      <Std color={livery} />
    </mesh>
  </group>
);

export const ControlTower: React.FC<{position?: V3; frame: number}> = ({position = [0, 0, 0], frame}) => (
  <group position={position}>
    <mesh position={[0, 3, 0]} castShadow>
      <cylinderGeometry args={[0.45, 0.7, 6, 12]} />
      <Std color="#e8ecf6" />
    </mesh>
    <mesh position={[0, 6.4, 0]} castShadow>
      <cylinderGeometry args={[1.3, 1.0, 0.9, 12]} />
      <meshStandardMaterial color="#7fd6ff" roughness={0.1} metalness={0.4} />
    </mesh>
    <mesh position={[0, 7, 0]}>
      <cylinderGeometry args={[1.4, 1.4, 0.15, 12]} />
      <Std color="#d6dbe8" />
    </mesh>
    <group position={[0, 7.4, 0]} rotation={[0, frame * 0.08, 0]}>
      <mesh position={[0.4, 0, 0]}>
        <boxGeometry args={[1.0, 0.25, 0.08]} />
        <Std color="#c9d1e6" />
      </mesh>
    </group>
  </group>
);

export const Container: React.FC<{position: V3; color: string; rotation?: number}> = ({position, color, rotation = 0}) => (
  <mesh position={position} rotation={[0, rotation, 0]} castShadow receiveShadow>
    <boxGeometry args={[2.4, 1, 1]} />
    <Std color={color} rough={0.7} />
  </mesh>
);

const CONTAINER_COLORS = ['#e4572e', '#29335c', '#f3a712', '#669bbc', '#a8c686', '#db3a34', '#2ec4b6'];

export const Ship: React.FC<{position?: V3; rotation?: number; seed?: string}> = ({position = [0, 0, 0], rotation = 0, seed = 'ship'}) => {
  const stacks = useMemo(() => {
    const out: {p: V3; c: string}[] = [];
    for (let x = 0; x < 7; x++)
      for (let z = 0; z < 3; z++) {
        const h = 1 + Math.floor(rnd(`${seed}${x}${z}`) * 3);
        for (let y = 0; y < h; y++) out.push({p: [-5.5 + x * 2.5, 1.7 + y * 1.0, -1.05 + z * 1.05], c: CONTAINER_COLORS[Math.floor(rnd(`${seed}c${x}${y}${z}`) * 7)]});
      }
    return out;
  }, [seed]);
  return (
    <group position={position} rotation={[0, rotation, 0]}>
      <mesh position={[0, 0.5, 0]} castShadow>
        <boxGeometry args={[20, 1.8, 3.8]} />
        <Std color="#22304f" />
      </mesh>
      <mesh position={[10.2, 0.5, 0]} rotation={[0, Math.PI / 4, 0]}>
        <boxGeometry args={[2.6, 1.8, 2.6]} />
        <Std color="#22304f" />
      </mesh>
      <mesh position={[0, -0.3, 0]}>
        <boxGeometry args={[20.1, 0.4, 3.85]} />
        <Std color="#b23a48" />
      </mesh>
      <mesh position={[-8.2, 2.6, 0]} castShadow>
        <boxGeometry args={[2.2, 2.6, 3.2]} />
        <Std color="#f2f4f8" />
      </mesh>
      {stacks.map((s, i) => (
        <Container key={i} position={s.p} color={s.c} />
      ))}
    </group>
  );
};

export const Crane: React.FC<{position?: V3; trolley?: number; color?: string}> = ({position = [0, 0, 0], trolley = 0.5, color = '#f3a712'}) => (
  <group position={position}>
    {[-1.6, 1.6].flatMap((x) =>
      [-1.6, 1.6].map((z) => (
        <mesh key={`${x}${z}`} position={[x, 5, z]} castShadow>
          <boxGeometry args={[0.3, 10, 0.3]} />
          <Std color={color} />
        </mesh>
      )),
    )}
    <mesh position={[0, 10.2, 3]} castShadow>
      <boxGeometry args={[0.6, 0.6, 16]} />
      <Std color={color} />
    </mesh>
    <mesh position={[0, 9.6, -2 + trolley * 12]}>
      <boxGeometry args={[1, 0.6, 1]} />
      <Std color="#3b4157" />
    </mesh>
  </group>
);

export const Truck: React.FC<{position?: V3; rotation?: number; color?: string; box?: string}> = ({position = [0, 0, 0], rotation = 0, color = '#e4572e', box = '#669bbc'}) => (
  <group position={position} rotation={[0, rotation, 0]}>
    <mesh position={[1.7, 0.75, 0]} castShadow>
      <boxGeometry args={[1.1, 1.2, 1.1]} />
      <Std color={color} />
    </mesh>
    <mesh position={[2.15, 0.95, 0]}>
      <boxGeometry args={[0.22, 0.45, 0.95]} />
      <meshStandardMaterial color="#bfe3ff" roughness={0.1} />
    </mesh>
    <mesh position={[-0.4, 0.95, 0]} castShadow>
      <boxGeometry args={[2.8, 1.2, 1.05]} />
      <Std color={box} />
    </mesh>
    {[-1.4, 0.2, 1.7].flatMap((x) =>
      [-0.5, 0.5].map((z) => (
        <mesh key={`${x}${z}`} position={[x, 0.22, z]} rotation={[Math.PI / 2, 0, 0]}>
          <cylinderGeometry args={[0.22, 0.22, 0.14, 12]} />
          <Std color="#1a1b24" />
        </mesh>
      )),
    )}
  </group>
);

export const Satellite: React.FC<{position?: V3; rotation?: V3; scale?: number}> = ({position = [0, 0, 0], rotation = [0, 0, 0], scale = 1}) => (
  <group position={position} rotation={rotation} scale={scale}>
    <mesh castShadow>
      <boxGeometry args={[0.7, 0.7, 0.9]} />
      <Std color="#d9b45a" metal={0.7} rough={0.3} />
    </mesh>
    {[-1, 1].map((s) => (
      <mesh key={s} position={[s * 1.3, 0, 0]}>
        <boxGeometry args={[1.7, 0.04, 0.7]} />
        <Std color="#2d4da3" metal={0.5} rough={0.25} emissive="#1b2f7a" ei={0.4} />
      </mesh>
    ))}
    <mesh position={[0, -0.5, 0]} rotation={[Math.PI, 0, 0]}>
      <coneGeometry args={[0.25, 0.3, 12, 1, true]} />
      <Std color="#eceff6" />
    </mesh>
  </group>
);

export const HospitalBuilding: React.FC<{position?: V3}> = ({position = [0, 0, 0]}) => (
  <group position={position}>
    <mesh position={[0, 3, 0]} castShadow receiveShadow>
      <boxGeometry args={[10, 6, 5]} />
      <Std color="#f4f6fb" />
    </mesh>
    <mesh position={[0, 4.2, 0]} castShadow>
      <boxGeometry args={[4, 8.4, 5.5]} />
      <Std color="#e8eef9" />
    </mesh>
    {Array.from({length: 24}).map((_, i) => (
      <mesh key={i} position={[-4.4 + (i % 8) * 1.25, 1.4 + Math.floor(i / 8) * 1.5, 2.51]}>
        <planeGeometry args={[0.7, 0.8]} />
        <meshStandardMaterial color="#86c5ff" emissive="#fff0c4" emissiveIntensity={0.25} />
      </mesh>
    ))}
    <group position={[0, 7, 2.8]}>
      <mesh>
        <boxGeometry args={[0.5, 1.6, 0.1]} />
        <Glow color={C.alert} k={1.2} />
      </mesh>
      <mesh>
        <boxGeometry args={[1.6, 0.5, 0.1]} />
        <Glow color={C.alert} k={1.2} />
      </mesh>
    </group>
  </group>
);

export const WaterPlant: React.FC<{position?: V3; frame: number}> = ({position = [0, 0, 0], frame}) => (
  <group position={position}>
    {[-3.2, 0, 3.2].map((x, i) => (
      <group key={x} position={[x, 0, 0]}>
        <mesh position={[0, 0.6, 0]} castShadow receiveShadow>
          <cylinderGeometry args={[1.4, 1.4, 1.2, 32]} />
          <Std color="#c9d1e6" />
        </mesh>
        <mesh position={[0, 1.18, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <circleGeometry args={[1.3, 32]} />
          <meshStandardMaterial color="#4fb3e8" roughness={0.1} metalness={0.2} />
        </mesh>
        <group position={[0, 1.3, 0]} rotation={[0, frame * 0.03 * (i % 2 ? -1 : 1), 0]}>
          <mesh>
            <boxGeometry args={[2.6, 0.08, 0.15]} />
            <Std color="#3b4157" />
          </mesh>
        </group>
      </group>
    ))}
    <mesh position={[0, 0.35, 2.2]} rotation={[0, 0, Math.PI / 2]}>
      <cylinderGeometry args={[0.18, 0.18, 9, 12]} />
      <Std color="#5f86c7" metal={0.4} />
    </mesh>
  </group>
);

/* The giant "off switch" that does not exist */
export const BigSwitch: React.FC<{position?: V3; on: number; scale?: number}> = ({position = [0, 0, 0], on, scale = 1}) => (
  <group position={position} scale={scale}>
    <mesh castShadow>
      <boxGeometry args={[2.2, 3.4, 0.5]} />
      <Std color="#e9edf7" rough={0.4} />
    </mesh>
    <mesh position={[0, 0, 0.26]}>
      <boxGeometry args={[1.0, 2.2, 0.1]} />
      <Std color="#2a2f45" />
    </mesh>
    <group position={[0, 0, 0.3]} rotation={[(on - 0.5) * -1.0, 0, 0]}>
      <mesh position={[0, 0, 0.35]}>
        <boxGeometry args={[0.7, 1.0, 0.7]} />
        <Std color={on > 0.5 ? C.ok : C.alert} ei={0.3} emissive={on > 0.5 ? C.ok : C.alert} />
      </mesh>
    </group>
  </group>
);

/* Volcanic island for the Tonga story */
export const Island: React.FC<{position?: V3; erupt: number; frame: number}> = ({position = [0, 0, 0], erupt, frame}) => (
  <group position={position}>
    <mesh position={[0, 0.2, 0]} receiveShadow>
      <cylinderGeometry args={[6, 7.5, 1, 24]} />
      <Std color="#e6cf98" />
    </mesh>
    <mesh position={[0, 2, 0]} castShadow>
      <coneGeometry args={[4.2, 4, 18, 1, true]} />
      <Std color="#6b5a54" flat />
    </mesh>
    {Array.from({length: 18}).map((_, i) => {
      const t = ((frame * 0.01 + i / 18) % 1) * erupt;
      const a = rnd('ash' + i) * Math.PI * 2;
      const r = t * (2 + rnd('asr' + i) * 4);
      return (
        <mesh key={i} position={[Math.cos(a) * r, 4 + t * 14, Math.sin(a) * r]} scale={(0.8 + t * 3) * erupt}>
          <icosahedronGeometry args={[1, 1]} />
          <meshStandardMaterial color={i % 3 ? '#8a8790' : '#b0aab0'} transparent opacity={(1 - t) * 0.85} flatShading />
        </mesh>
      );
    })}
    {Array.from({length: 5}).map((_, i) => (
      <mesh key={'p' + i} position={[-4 + i * 2, 1.2, 4.5 - (i % 2)]} castShadow>
        <coneGeometry args={[0.6, 1.4, 6]} />
        <Std color="#4f9d69" flat />
      </mesh>
    ))}
  </group>
);

/* Ocean surface plane */
export const Ocean: React.FC<{color?: string; size?: number; y?: number}> = ({color = '#1f6fa8', size = 400, y = 0}) => (
  <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, y, 0]} receiveShadow>
    <planeGeometry args={[size, size]} />
    <meshStandardMaterial color={color} roughness={0.25} metalness={0.15} />
  </mesh>
);
