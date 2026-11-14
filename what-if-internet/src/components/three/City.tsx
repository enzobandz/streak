import React, {useMemo} from 'react';
import * as THREE from 'three';
import {rnd, rndRange} from '../../lib/anim';
import {BUILDINGS, C, SHIRTS} from '../../theme';

/* ---------- shared procedural window textures (built once, deterministic) ---------- */
let TEX: {map: THREE.Texture; emissive: THREE.Texture} | null = null;
const windowTextures = () => {
  if (TEX) return TEX;
  const W = 128, H = 256, cols = 4, rows = 8;
  const mk = () => {
    const c = document.createElement('canvas');
    c.width = W;
    c.height = H;
    return c;
  };
  const a = mk(), b = mk();
  const ga = a.getContext('2d')!, gb = b.getContext('2d')!;
  ga.fillStyle = '#ffffff';
  ga.fillRect(0, 0, W, H);
  gb.fillStyle = '#000000';
  gb.fillRect(0, 0, W, H);
  for (let r = 0; r < rows; r++) {
    for (let cc = 0; cc < cols; cc++) {
      const x = cc * (W / cols) + 6, y = r * (H / rows) + 7, w = W / cols - 12, h = H / rows - 12;
      ga.fillStyle = '#7d8fb8';
      ga.fillRect(x, y, w, h);
      const lit = rnd(`win${r}-${cc}`) < 0.55;
      if (lit) {
        gb.fillStyle = rnd(`wc${r}-${cc}`) < 0.75 ? '#ffd58a' : '#bfe9ff';
        gb.fillRect(x, y, w, h);
      }
    }
  }
  const map = new THREE.CanvasTexture(a);
  const emissive = new THREE.CanvasTexture(b);
  for (const t of [map, emissive]) {
    t.wrapS = t.wrapT = THREE.RepeatWrapping;
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = 4;
  }
  TEX = {map, emissive};
  return TEX;
};

export const Building: React.FC<{
  w: number;
  d: number;
  h: number;
  color: string;
  position: [number, number, number];
  night?: number; // 0 day … 1 night (window glow)
  roof?: boolean;
}> = ({w, d, h, color, position, night = 0, roof = true}) => {
  const {map, emissive} = windowTextures();
  const mats = useMemo(() => {
    const m = map.clone();
    const e = emissive.clone();
    m.repeat.set(Math.max(1, Math.round(w / 1.6)) / 4, Math.max(1, Math.round(h / 1.1)) / 8);
    e.repeat.copy(m.repeat);
    m.offset.set(0, 0);
    m.needsUpdate = e.needsUpdate = true;
    return {m, e};
  }, [w, h, map, emissive]);
  return (
    <group position={position}>
      <mesh position={[0, h / 2, 0]} castShadow receiveShadow>
        <boxGeometry args={[w, h, d]} />
        <meshStandardMaterial color={color} map={mats.m} emissiveMap={mats.e} emissive="#ffffff" emissiveIntensity={night * 1.1} roughness={0.75} />
      </mesh>
      {roof ? (
        <>
          <mesh position={[0, h + 0.06, 0]} castShadow>
            <boxGeometry args={[w + 0.12, 0.12, d + 0.12]} />
            <meshStandardMaterial color={new THREE.Color(color).multiplyScalar(0.8)} roughness={0.8} />
          </mesh>
          <mesh position={[w * 0.18, h + 0.32, -d * 0.15]} castShadow>
            <boxGeometry args={[w * 0.3, 0.45, d * 0.3]} />
            <meshStandardMaterial color="#9aa3bd" roughness={0.8} />
          </mesh>
        </>
      ) : null}
    </group>
  );
};

export const Tree: React.FC<{position: [number, number, number]; s?: number; color?: string}> = ({position, s = 1, color = '#58b86a'}) => (
  <group position={position} scale={s}>
    <mesh position={[0, 0.45, 0]} castShadow>
      <cylinderGeometry args={[0.07, 0.1, 0.9, 6]} />
      <meshStandardMaterial color="#7a5435" />
    </mesh>
    <mesh position={[0, 1.25, 0]} castShadow>
      <icosahedronGeometry args={[0.6, 0]} />
      <meshStandardMaterial color={color} flatShading roughness={0.8} />
    </mesh>
  </group>
);

export const Lamp: React.FC<{position: [number, number, number]; on?: number}> = ({position, on = 0}) => (
  <group position={position}>
    <mesh position={[0, 1.4, 0]}>
      <cylinderGeometry args={[0.04, 0.05, 2.8, 6]} />
      <meshStandardMaterial color="#3b4157" />
    </mesh>
    <mesh position={[0, 2.85, 0]}>
      <sphereGeometry args={[0.13, 10, 8]} />
      <meshStandardMaterial color="#fff2c4" emissive="#ffd27a" emissiveIntensity={on * 3} />
    </mesh>
  </group>
);

export const Car: React.FC<{position: [number, number, number]; rotation?: number; color: string; kind?: 'car' | 'van' | 'taxi'; lights?: number}> = ({
  position,
  rotation = 0,
  color,
  kind = 'car',
  lights = 0,
}) => {
  const len = kind === 'van' ? 1.9 : 1.6;
  return (
    <group position={position} rotation={[0, rotation, 0]}>
      <mesh position={[0, 0.32, 0]} castShadow>
        <boxGeometry args={[len, 0.4, 0.8]} />
        <meshStandardMaterial color={color} roughness={0.35} metalness={0.2} />
      </mesh>
      <mesh position={[kind === 'van' ? 0.15 : -0.05, kind === 'van' ? 0.72 : 0.66, 0]} castShadow>
        <boxGeometry args={[kind === 'van' ? 1.5 : 0.9, kind === 'van' ? 0.5 : 0.36, 0.72]} />
        <meshStandardMaterial color={kind === 'van' ? color : '#cfe3ff'} roughness={0.15} metalness={0.3} />
      </mesh>
      {kind === 'taxi' ? (
        <mesh position={[-0.05, 0.9, 0]}>
          <boxGeometry args={[0.3, 0.1, 0.2]} />
          <meshStandardMaterial color="#fff" emissive="#fff3a0" emissiveIntensity={0.6} />
        </mesh>
      ) : null}
      {[-1, 1].flatMap((sx) =>
        [-1, 1].map((sz) => (
          <mesh key={`${sx}${sz}`} position={[sx * len * 0.3, 0.16, sz * 0.4]} rotation={[Math.PI / 2, 0, 0]}>
            <cylinderGeometry args={[0.16, 0.16, 0.1, 12]} />
            <meshStandardMaterial color="#1a1b24" />
          </mesh>
        )),
      )}
      {[-1, 1].map((sz) => (
        <mesh key={sz} position={[len / 2 + 0.01, 0.36, sz * 0.26]}>
          <boxGeometry args={[0.02, 0.08, 0.14]} />
          <meshStandardMaterial color="#fffbe6" emissive="#fff1b0" emissiveIntensity={lights * 2.5} />
        </mesh>
      ))}
    </group>
  );
};

/** Billboard / screen with an emissive face whose colour the scene drives. */
export const Screen: React.FC<{position: [number, number, number]; rotation?: number; w: number; h: number; color: string; glow?: number; children?: React.ReactNode}> = ({
  position,
  rotation = 0,
  w,
  h,
  color,
  glow = 1,
  children,
}) => (
  <group position={position} rotation={[0, rotation, 0]}>
    <mesh>
      <boxGeometry args={[w + 0.2, h + 0.2, 0.15]} />
      <meshStandardMaterial color="#1c2033" roughness={0.5} />
    </mesh>
    <mesh position={[0, 0, 0.08]}>
      <planeGeometry args={[w, h]} />
      <meshBasicMaterial color={new THREE.Color(color).multiplyScalar(glow)} toneMapped={false} />
    </mesh>
    {children}
  </group>
);

/* ---------- Street: a main road along X with sidewalks, buildings on both sides ---------- */
export const Street: React.FC<{frame: number; length?: number; night?: number; traffic?: number; seed?: string; lampsOn?: number; nearSide?: boolean}> = ({
  frame,
  length = 80,
  night = 0,
  traffic = 1,
  seed = 'st',
  lampsOn = 0,
  nearSide = true,
}) => {
  const blds = useMemo(() => {
    const out: {x: number; z: number; w: number; d: number; h: number; color: string}[] = [];
    for (const side of nearSide ? [-1, 1] : [-1]) {
      let x = -length / 2;
      let i = 0;
      while (x < length / 2) {
        const w = rndRange(`${seed}w${side}${i}`, 3, 5.5);
        const h = rndRange(`${seed}h${side}${i}`, 4, side < 0 ? 14 : 9);
        const d = rndRange(`${seed}d${side}${i}`, 4, 6);
        out.push({x: x + w / 2, z: side * (6.2 + d / 2), w, d, h, color: BUILDINGS[Math.floor(rnd(`${seed}c${side}${i}`) * BUILDINGS.length)]});
        x += w + 0.25;
        i++;
      }
    }
    return out;
  }, [length, seed, nearSide]);
  const cars = useMemo(
    () =>
      Array.from({length: 14}).map((_, i) => ({
        lane: i % 2 === 0 ? 1 : -1,
        off: rnd(`${seed}car${i}`) * length,
        color: SHIRTS[i % SHIRTS.length],
        kind: (i % 5 === 0 ? 'van' : i % 4 === 0 ? 'taxi' : 'car') as 'car' | 'van' | 'taxi',
        speed: 0.09 + rnd(`${seed}cs${i}`) * 0.05,
      })),
    [length, seed],
  );
  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow position={[0, 0, 0]}>
        <planeGeometry args={[length + 40, 80]} />
        <meshStandardMaterial color="#8fa38a" roughness={1} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow position={[0, 0.01, 0]}>
        <planeGeometry args={[length + 40, 6]} />
        <meshStandardMaterial color={C.road} roughness={0.9} />
      </mesh>
      {[-1, 1].map((s) => (
        <mesh key={s} rotation={[-Math.PI / 2, 0, 0]} receiveShadow position={[0, 0.06, s * 4.6]}>
          <planeGeometry args={[length + 40, 3.2]} />
          <meshStandardMaterial color="#c9ccd8" roughness={0.9} />
        </mesh>
      ))}
      {Array.from({length: Math.floor((length + 40) / 3)}).map((_, i) => (
        <mesh key={i} rotation={[-Math.PI / 2, 0, 0]} position={[-(length + 40) / 2 + i * 3, 0.02, 0]}>
          <planeGeometry args={[1.4, 0.12]} />
          <meshBasicMaterial color="#e8e2c8" />
        </mesh>
      ))}
      {blds.map((b, i) => (
        <Building key={i} w={b.w} d={b.d} h={b.h} color={b.color} position={[b.x, 0, b.z]} night={night} />
      ))}
      {Array.from({length: Math.floor(length / 8)}).map((_, i) => (
        <group key={i}>
          <Tree position={[-length / 2 + i * 8 + 2, 0.06, -5.6]} s={0.9} />
          <Lamp position={[-length / 2 + i * 8 + 6, 0.06, 5.6]} on={lampsOn} />
        </group>
      ))}
      {cars.map((c, i) => {
        const travel = frame * c.speed * traffic;
        const x = (((c.off + travel * c.lane) % length) + length) % length - length / 2;
        return <Car key={i} position={[x, 0, c.lane * 1.4]} rotation={c.lane > 0 ? 0 : Math.PI} color={c.color} kind={c.kind} lights={night} />;
      })}
    </group>
  );
};

/* ---------- Aerial skyline: grid of blocks ---------- */
export const Skyline: React.FC<{size?: number; seed?: string; night?: number; frame?: number; traffic?: number; ground?: string}> = ({
  size = 7,
  seed = 'sky',
  night = 0,
  frame = 0,
  traffic = 1,
  ground = '#7e9a86',
}) => {
  const block = 8;
  const items = useMemo(() => {
    const out: {x: number; z: number; w: number; d: number; h: number; color: string; park: boolean}[] = [];
    for (let bx = -size; bx <= size; bx++) {
      for (let bz = -size; bz <= size; bz++) {
        const park = rnd(`${seed}p${bx}${bz}`) < 0.08;
        const dist = Math.hypot(bx, bz);
        for (let k = 0; k < 4; k++) {
          const ox = (k % 2) * 3 - 1.5, oz = Math.floor(k / 2) * 3 - 1.5;
          const h = park ? 0 : rndRange(`${seed}h${bx}${bz}${k}`, 2, 4) + Math.max(0, 9 - dist * 1.2) * rnd(`${seed}t${bx}${bz}${k}`) * 2;
          out.push({x: bx * block + ox, z: bz * block + oz, w: 2.6, d: 2.6, h, color: BUILDINGS[Math.floor(rnd(`${seed}c${bx}${bz}${k}`) * BUILDINGS.length)], park});
        }
      }
    }
    return out;
  }, [size, seed]);
  const extent = (size * 2 + 1) * block;
  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow position={[0, -0.02, 0]}>
        <planeGeometry args={[extent + 200, extent + 200]} />
        <meshStandardMaterial color={ground} roughness={1} />
      </mesh>
      {Array.from({length: size * 2 + 2}).map((_, i) => {
        const p = (i - size - 0.5) * block;
        return (
          <group key={i}>
            <mesh rotation={[-Math.PI / 2, 0, 0]} position={[p, 0, 0]} receiveShadow>
              <planeGeometry args={[1.6, extent]} />
              <meshStandardMaterial color={C.road} />
            </mesh>
            <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.005, p]} receiveShadow>
              <planeGeometry args={[extent, 1.6]} />
              <meshStandardMaterial color={C.road} />
            </mesh>
          </group>
        );
      })}
      {items.map((b, i) =>
        b.park ? (
          i % 4 === 0 ? <Tree key={i} position={[b.x + 1.5, 0, b.z + 1.5]} s={1.6} /> : null
        ) : (
          <Building key={i} w={b.w} d={b.d} h={b.h} color={b.color} position={[b.x, 0, b.z]} night={night} roof={b.h > 5} />
        ),
      )}
      {Array.from({length: 40}).map((_, i) => {
        const road = (Math.floor(rnd(`${seed}r${i}`) * (size * 2 + 2)) - size - 0.5) * block;
        const horiz = i % 2 === 0;
        const dir = rnd(`${seed}dir${i}`) < 0.5 ? 1 : -1;
        const t = (((rnd(`${seed}o${i}`) * extent + frame * 0.12 * traffic * dir) % extent) + extent) % extent - extent / 2;
        const lane = dir * 0.35;
        return (
          <Car
            key={i}
            position={horiz ? [t, 0, road + lane] : [road + lane, 0, t]}
            rotation={horiz ? (dir > 0 ? 0 : Math.PI) : dir > 0 ? -Math.PI / 2 : Math.PI / 2}
            color={SHIRTS[i % SHIRTS.length]}
            lights={night}
          />
        );
      })}
    </group>
  );
};
