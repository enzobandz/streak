import React, {useLayoutEffect, useMemo, useRef} from 'react';
import * as THREE from 'three';
import dots from '../../data/globe-dots.json';
import {rnd, clamp} from '../../lib/anim';
import {C} from '../../theme';

export const R = 2;

export const latLon = (lat: number, lon: number, r = R) => {
  const phi = ((90 - lat) * Math.PI) / 180;
  const th = ((lon + 180) * Math.PI) / 180;
  return new THREE.Vector3(-r * Math.sin(phi) * Math.cos(th), r * Math.cos(phi), r * Math.sin(phi) * Math.sin(th));
};

// Approximate coastal landing cities. Routes are ILLUSTRATIVE (great-circle arcs), not surveyed cable paths.
export const PLACES: Record<string, [number, number]> = {
  newYork: [40.7, -74.0],
  virginia: [36.85, -75.98],
  bude: [50.8, -4.5],
  bilbao: [43.3, -2.9],
  lisbon: [38.7, -9.1],
  miami: [25.8, -80.2],
  fortaleza: [-3.7, -38.5],
  marseille: [43.3, 5.4],
  alexandria: [31.2, 29.9],
  djibouti: [11.6, 43.1],
  mumbai: [19.1, 72.9],
  singapore: [1.3, 103.8],
  hongKong: [22.3, 114.2],
  tokyo: [35.7, 139.7],
  losAngeles: [34.0, -118.2],
  honolulu: [21.3, -157.9],
  sydney: [-33.9, 151.2],
  capeTown: [-33.9, 18.4],
  lagos: [6.5, 3.4],
  suva: [-18.1, 178.4],
  tonga: [-21.1, -175.2],
  cairo: [30.0, 31.2],
};

export const CABLES: [string, string][] = [
  ['newYork', 'bude'],
  ['virginia', 'bilbao'],
  ['miami', 'fortaleza'],
  ['fortaleza', 'lisbon'],
  ['lisbon', 'lagos'],
  ['lagos', 'capeTown'],
  ['marseille', 'alexandria'],
  ['alexandria', 'djibouti'],
  ['djibouti', 'mumbai'],
  ['mumbai', 'singapore'],
  ['singapore', 'hongKong'],
  ['hongKong', 'tokyo'],
  ['tokyo', 'losAngeles'],
  ['losAngeles', 'honolulu'],
  ['honolulu', 'sydney'],
  ['sydney', 'singapore'],
  ['sydney', 'suva'],
  ['suva', 'tonga'],
  ['capeTown', 'djibouti'],
  ['newYork', 'miami'],
];

export const arcCurve = (a: [number, number], b: [number, number]) => {
  const va = latLon(a[0], a[1]).normalize();
  const vb = latLon(b[0], b[1]).normalize();
  const ang = va.angleTo(vb);
  const pts: THREE.Vector3[] = [];
  const n = 48;
  for (let i = 0; i <= n; i++) {
    const t = i / n;
    const v = new THREE.Vector3().copy(va).lerp(vb, t).normalize();
    // proper slerp for long arcs
    if (ang > 0.01) {
      const s = Math.sin(ang);
      v.copy(va).multiplyScalar(Math.sin((1 - t) * ang) / s).add(vb.clone().multiplyScalar(Math.sin(t * ang) / s));
    }
    const h = 1 + (0.012 + 0.11 * (ang / Math.PI)) * Math.sin(Math.PI * t);
    pts.push(v.multiplyScalar(R * h));
  }
  return new THREE.CatmullRomCurve3(pts);
};

const atmosphereMat = () =>
  new THREE.ShaderMaterial({
    uniforms: {color: {value: new THREE.Color(C.data)}, strength: {value: 1}},
    vertexShader: `varying vec3 vN; varying vec3 vV;
      void main(){ vec4 mv = modelViewMatrix*vec4(position,1.0); vN = normalize(normalMatrix*normal); vV = normalize(-mv.xyz); gl_Position = projectionMatrix*mv; }`,
    fragmentShader: `uniform vec3 color; uniform float strength; varying vec3 vN; varying vec3 vV;
      void main(){ float d = abs(dot(vN, vV)); float f = pow(smoothstep(0.0, 0.56, d), 3.0); gl_FragColor = vec4(color, f*0.55*strength); }`,
    blending: THREE.AdditiveBlending,
    transparent: true,
    side: THREE.BackSide,
    depthWrite: false,
  });

const rimMat = () =>
  new THREE.ShaderMaterial({
    uniforms: {color: {value: new THREE.Color(C.data)}, strength: {value: 1}},
    vertexShader: `varying vec3 vN; varying vec3 vV;
      void main(){ vec4 mv = modelViewMatrix*vec4(position,1.0); vN = normalize(normalMatrix*normal); vV = normalize(-mv.xyz); gl_Position = projectionMatrix*mv; }`,
    fragmentShader: `uniform vec3 color; uniform float strength; varying vec3 vN; varying vec3 vV;
      void main(){ float f = pow(1.0 - max(dot(vN, vV), 0.0), 3.2); gl_FragColor = vec4(color, f*0.85*strength); }`,
    blending: THREE.AdditiveBlending,
    transparent: true,
    depthWrite: false,
  });

export type Marker = {lat: number; lon: number; color: string; strength: number; size?: number};

export const Globe: React.FC<{
  frame: number;
  rotation: number; // y rotation (radians)
  tilt?: number;
  connectivity?: number; // 1 live, 0 dead
  cableDraw?: number; // 0..1 draw-on
  dimCables?: string[]; // cable keys "a-b" to force dark (e.g. Tonga)
  markers?: Marker[];
  landGlow?: number;
  position?: [number, number, number];
  scale?: number;
}> = ({frame, rotation, tilt = 0.32, connectivity = 1, cableDraw = 1, dimCables = [], markers = [], landGlow = 1, position = [0, 0, 0], scale = 1}) => {
  const inst = useRef<THREE.InstancedMesh>(null);
  const atmo = useMemo(atmosphereMat, []);
  const rim = useMemo(rimMat, []);
  const curves = useMemo(() => CABLES.map(([a, b]) => ({key: `${a}-${b}`, curve: arcCurve(PLACES[a], PLACES[b])})), []);
  const tubes = useMemo(() => curves.map((c) => new THREE.TubeGeometry(c.curve, 64, 0.0065, 5, false)), [curves]);

  useLayoutEffect(() => {
    const m = inst.current;
    if (!m) return;
    const d = new THREE.Object3D();
    const col = new THREE.Color();
    (dots as number[][]).forEach(([lat, lon], i) => {
      const p = latLon(lat, lon, R * 1.002);
      d.position.copy(p);
      d.lookAt(p.clone().multiplyScalar(2));
      const s = 0.85 + rnd('ds' + i) * 0.4;
      d.scale.set(s, s, s);
      d.updateMatrix();
      m.setMatrixAt(i, d.matrix);
      const city = rnd('city' + i) < 0.09;
      col.set(city ? C.amber : '#5E7BC9').multiplyScalar(city ? 1.25 : 0.8 + rnd('c' + i) * 0.35);
      m.setColorAt(i, col);
    });
    m.instanceMatrix.needsUpdate = true;
    if (m.instanceColor) m.instanceColor.needsUpdate = true;
  }, []);

  atmo.uniforms.color.value.set(connectivity > 0.5 ? C.data : C.dataSoft);
  atmo.uniforms.strength.value = 0.45 + 0.55 * connectivity;
  rim.uniforms.color.value.set(connectivity > 0.5 ? C.data : C.dataSoft);
  rim.uniforms.strength.value = 0.5 + 0.5 * connectivity;
  const live = new THREE.Color(C.data);
  const dead = new THREE.Color('#5a2236');
  const cableCol = dead.clone().lerp(live, connectivity);

  return (
    <group position={position} scale={scale} rotation={[tilt, 0, 0]}>
      <group rotation={[0, rotation, 0]}>
        <mesh>
          <sphereGeometry args={[R, 96, 64]} />
          <meshStandardMaterial color="#0B1638" emissive="#0a1b46" emissiveIntensity={0.55} roughness={0.85} metalness={0.05} />
        </mesh>
        <instancedMesh ref={inst} args={[undefined, undefined, (dots as number[][]).length]}>
          <circleGeometry args={[0.0125, 6]} />
          <meshBasicMaterial toneMapped={false} transparent opacity={0.55 + 0.45 * landGlow} />
        </instancedMesh>
        {curves.map((c, i) => {
          const forcedDark = dimCables.includes(c.key);
          const show = clamp(cableDraw * 1.6 - (i / curves.length) * 0.6);
          if (show <= 0) return null;
          const geo = tubes[i];
          const total = geo.index ? geo.index.count : 0;
          const conn = forcedDark ? 0 : connectivity;
          const col = forcedDark ? new THREE.Color('#5a2236') : cableCol;
          geo.setDrawRange(0, Math.floor((total * show) / 3) * 3);
          return (
            <group key={c.key}>
              <mesh geometry={geo}>
                <meshBasicMaterial color={col} toneMapped={false} transparent opacity={0.35 + 0.55 * conn} />
              </mesh>
              {conn > 0.02 && show >= 1
                ? [0, 1, 2].map((k) => {
                    const t = (rnd(c.key + k) + frame * (0.006 + rnd(c.key) * 0.004) + k / 3) % 1;
                    const p = c.curve.getPointAt(t);
                    return (
                      <mesh key={k} position={p} scale={0.028 * (0.6 + 0.4 * conn)}>
                        <sphereGeometry args={[1, 10, 8]} />
                        <meshBasicMaterial color="#C8F6FF" toneMapped={false} transparent opacity={conn} />
                      </mesh>
                    );
                  })
                : null}
            </group>
          );
        })}
        {markers.map((mk, i) => {
          const p = latLon(mk.lat, mk.lon, R * 1.006);
          const ring = (frame * 0.025 + i * 0.3) % 1;
          return (
            <group key={i} position={p} quaternion={new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), p.clone().normalize())}>
              <mesh>
                <circleGeometry args={[0.04 * (mk.size ?? 1), 20]} />
                <meshBasicMaterial color={mk.color} toneMapped={false} transparent opacity={mk.strength} side={THREE.DoubleSide} />
              </mesh>
              <mesh scale={1 + ring * 3}>
                <ringGeometry args={[0.05 * (mk.size ?? 1), 0.062 * (mk.size ?? 1), 32]} />
                <meshBasicMaterial color={mk.color} toneMapped={false} transparent opacity={mk.strength * (1 - ring)} side={THREE.DoubleSide} />
              </mesh>
            </group>
          );
        })}
      </group>
      <mesh scale={1.012}>
        <sphereGeometry args={[R, 64, 48]} />
        <primitive object={rim} attach="material" />
      </mesh>
      <mesh scale={1.13}>
        <sphereGeometry args={[R, 64, 48]} />
        <primitive object={atmo} attach="material" />
      </mesh>
    </group>
  );
};

export const Stars: React.FC<{count?: number; radius?: number; opacity?: number}> = ({count = 1500, radius = 120, opacity = 1}) => {
  const geo = useMemo(() => {
    const g = new THREE.BufferGeometry();
    const arr = new Float32Array(count * 3);
    for (let i = 0; i < count; i++) {
      const u = rnd('su' + i) * 2 - 1;
      const t = rnd('st' + i) * Math.PI * 2;
      const s = Math.sqrt(1 - u * u);
      arr.set([radius * s * Math.cos(t), radius * u, radius * s * Math.sin(t)], i * 3);
    }
    g.setAttribute('position', new THREE.BufferAttribute(arr, 3));
    return g;
  }, [count, radius]);
  return (
    <points geometry={geo}>
      <pointsMaterial color="#cfd9ff" size={1.6} sizeAttenuation={false} transparent opacity={0.8 * opacity} toneMapped={false} />
    </points>
  );
};
