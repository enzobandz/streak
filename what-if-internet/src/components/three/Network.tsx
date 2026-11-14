import React, {useLayoutEffect, useMemo, useRef} from 'react';
import * as THREE from 'three';
import {C} from '../../theme';
import {clamp, rnd} from '../../lib/anim';

export const NET_TYPES = [
  {key: 'isp', color: C.data},
  {key: 'uni', color: C.amber},
  {key: 'cloud', color: C.violet},
  {key: 'gov', color: C.ok},
];

export type NetNode = {p: THREE.Vector3; type: number; size: number};

export const buildGraph = (seed = 'g', clusters = 9, perCluster = 9, radius = 7) => {
  const nodes: NetNode[] = [];
  const centers: THREE.Vector3[] = [];
  for (let c = 0; c < clusters; c++) {
    const a = (c / clusters) * Math.PI * 2 + rnd(seed + 'ca' + c) * 0.4;
    const r = radius * (0.55 + rnd(seed + 'cr' + c) * 0.45);
    centers.push(new THREE.Vector3(Math.cos(a) * r, (rnd(seed + 'cy' + c) - 0.5) * 3, Math.sin(a) * r));
  }
  centers.forEach((cc, c) => {
    for (let i = 0; i < perCluster; i++) {
      const p = cc.clone().add(new THREE.Vector3((rnd(`${seed}x${c}${i}`) - 0.5) * 3.2, (rnd(`${seed}y${c}${i}`) - 0.5) * 2.4, (rnd(`${seed}z${c}${i}`) - 0.5) * 3.2));
      nodes.push({p, type: Math.floor(rnd(`${seed}t${c}${i}`) * 4), size: i === 0 ? 0.28 : 0.1 + rnd(`${seed}s${c}${i}`) * 0.1});
    }
  });
  const edges: [number, number][] = [];
  const has = new Set<string>();
  nodes.forEach((n, i) => {
    const d = nodes.map((m, j) => ({j, d: i === j ? 1e9 : n.p.distanceTo(m.p)})).sort((a, b) => a.d - b.d);
    for (let k = 0; k < 2; k++) {
      const j = d[k].j;
      const key = i < j ? `${i}-${j}` : `${j}-${i}`;
      if (!has.has(key)) {
        has.add(key);
        edges.push([i, j]);
      }
    }
  });
  // cluster hubs link to each other (and to the exchange at origin)
  for (let c = 0; c < clusters; c++) {
    const a = c * perCluster, b = ((c + 1) % clusters) * perCluster;
    edges.push([a, b]);
  }
  return {nodes, edges, hubs: centers.map((_, c) => c * perCluster)};
};

export const NetworkGraph: React.FC<{
  frame: number;
  graph: ReturnType<typeof buildGraph>;
  reveal?: number; // 0..1 nodes appear
  linkReveal?: number; // 0..1 edges appear
  connectivity?: number; // 1 live, 0 cut (islands)
  islandSpread?: number; // push clusters apart when cut
  exchange?: number; // 0..1 central IXP visibility
  route?: number[]; // node indices of a highlighted BGP path (-1 = exchange)
  routeProgress?: number;
  packets?: number;
}> = ({frame, graph, reveal = 1, linkReveal = 1, connectivity = 1, islandSpread = 0, exchange = 0, route, routeProgress = 0, packets = 1}) => {
  const inst = useRef<THREE.InstancedMesh>(null);
  const {nodes, edges, hubs} = graph;
  const perCluster = nodes.length / hubs.length;
  const pos = (i: number) => {
    if (i < 0) return new THREE.Vector3(0, 0, 0);
    const n = nodes[i];
    const c = Math.floor(i / perCluster);
    const hub = nodes[hubs[c]].p;
    return n.p.clone().add(hub.clone().setY(0).normalize().multiplyScalar(islandSpread * 3));
  };
  useLayoutEffect(() => {
    const m = inst.current;
    if (!m) return;
    const d = new THREE.Object3D();
    const col = new THREE.Color();
    nodes.forEach((n, i) => {
      const appear = clamp((reveal - (i / nodes.length) * 0.8) * 5);
      d.position.copy(pos(i));
      d.scale.setScalar(n.size * appear * (1 + 0.15 * Math.sin(frame * 0.1 + i)));
      d.updateMatrix();
      m.setMatrixAt(i, d.matrix);
      col.set(NET_TYPES[n.type].color).multiplyScalar(0.7 + 0.8 * connectivity);
      m.setColorAt(i, col);
    });
    m.instanceMatrix.needsUpdate = true;
    if (m.instanceColor) m.instanceColor.needsUpdate = true;
  });
  const exchangeEdges: [number, number][] = exchange > 0 ? hubs.map((h) => [h, -1]) : [];
  const allEdges = [...edges, ...exchangeEdges];
  const lineGeo = useMemo(() => new THREE.BufferGeometry(), []);
  const n = allEdges.length;
  const arr = new Float32Array(n * 6);
  const cols = new Float32Array(n * 6);
  const live = new THREE.Color(C.dataSoft);
  const dead = new THREE.Color('#4a1f33');
  allEdges.forEach(([a, b], k) => {
    const pa = pos(a), pb = pos(b);
    const isEx = b === -1;
    const vis = isEx ? exchange : clamp((linkReveal - (k / edges.length) * 0.7) * 4);
    const crossCluster = Math.floor(a / perCluster) !== Math.floor((b < 0 ? a : b) / perCluster) || isEx;
    const cc = crossCluster ? dead.clone().lerp(live, connectivity) : live;
    const pe = pa.clone().lerp(pb, vis);
    arr.set([pa.x, pa.y, pa.z, pe.x, pe.y, pe.z], k * 6);
    const br = crossCluster ? 0.4 + 0.6 * connectivity : 1;
    cols.set([cc.r * br, cc.g * br, cc.b * br, cc.r * br, cc.g * br, cc.b * br], k * 6);
  });
  lineGeo.setAttribute('position', new THREE.BufferAttribute(arr, 3));
  lineGeo.setAttribute('color', new THREE.BufferAttribute(cols, 3));
  lineGeo.computeBoundingSphere();

  // route highlight
  const routePts = route ? route.map(pos) : [];
  const routeSegs = routePts.length - 1;
  const rp = routeProgress * routeSegs;
  return (
    <group>
      <instancedMesh ref={inst} args={[undefined, undefined, nodes.length]}>
        <sphereGeometry args={[1, 16, 12]} />
        <meshStandardMaterial emissive="#ffffff" emissiveIntensity={0.06} roughness={0.35} toneMapped={false} />
      </instancedMesh>
      <lineSegments geometry={lineGeo}>
        <lineBasicMaterial vertexColors transparent opacity={0.9} toneMapped={false} />
      </lineSegments>
      {exchange > 0 ? (
        <group scale={exchange}>
          <mesh>
            <icosahedronGeometry args={[0.7, 1]} />
            <meshStandardMaterial color="#ffffff" emissive={C.data} emissiveIntensity={0.9} flatShading />
          </mesh>
          <mesh rotation={[Math.PI / 2, 0, frame * 0.02]}>
            <torusGeometry args={[1.2, 0.04, 8, 64]} />
            <meshBasicMaterial color={C.data} toneMapped={false} />
          </mesh>
          <mesh rotation={[Math.PI / 2.4, frame * 0.015, 0]}>
            <torusGeometry args={[1.5, 0.025, 8, 64]} />
            <meshBasicMaterial color={C.amber} toneMapped={false} />
          </mesh>
        </group>
      ) : null}
      {/* packets on intra-cluster links */}
      {connectivity > 0 || packets > 0
        ? edges.slice(0, 60).map(([a, b], k) => {
            const t = (frame * 0.02 + rnd('pk' + k)) % 1;
            const crossCluster = Math.floor(a / perCluster) !== Math.floor(b / perCluster);
            const o = (crossCluster ? connectivity : 1) * packets * clamp(linkReveal * 2 - 1);
            if (o <= 0.02) return null;
            const p = pos(a).lerp(pos(b), t);
            return (
              <mesh key={k} position={p} scale={0.06}>
                <sphereGeometry args={[1, 8, 6]} />
                <meshBasicMaterial color="#d6fbff" toneMapped={false} transparent opacity={o} />
              </mesh>
            );
          })
        : null}
      {routeSegs > 0
        ? routePts.slice(0, -1).map((pa, i) => {
            const seg = clamp(rp - i);
            if (seg <= 0) return null;
            const pb = routePts[i + 1];
            const mid = pa.clone().lerp(pb, seg);
            const dir = mid.clone().sub(pa);
            const len = dir.length();
            const q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.clone().normalize());
            return (
              <group key={i}>
                <mesh position={pa.clone().lerp(mid, 0.5)} quaternion={q}>
                  <cylinderGeometry args={[0.06, 0.06, len, 8]} />
                  <meshBasicMaterial color={C.amber} toneMapped={false} />
                </mesh>
                <mesh position={pa}>
                  <sphereGeometry args={[0.24, 16, 12]} />
                  <meshBasicMaterial color={C.amber} toneMapped={false} transparent opacity={0.6} />
                </mesh>
              </group>
            );
          })
        : null}
      {routeSegs > 0 && routeProgress > 0 && routeProgress < 1 ? (
        <mesh position={routePts[Math.min(routeSegs - 1, Math.floor(rp))].clone().lerp(routePts[Math.min(routeSegs, Math.floor(rp) + 1)], rp - Math.floor(rp))} scale={0.2}>
          <sphereGeometry args={[1, 12, 10]} />
          <meshBasicMaterial color="#ffffff" toneMapped={false} />
        </mesh>
      ) : null}
    </group>
  );
};

/* ---------- Seabed with a submarine cable ---------- */
export const Seabed: React.FC<{frame: number; pulse?: number}> = ({frame, pulse = 1}) => {
  const geo = useMemo(() => {
    const g = new THREE.PlaneGeometry(200, 200, 160, 160);
    const p = g.attributes.position as THREE.BufferAttribute;
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), y = p.getY(i);
      p.setZ(i, Math.sin(x * 0.11) * 0.8 + Math.sin(y * 0.07 + 1) * 1.1 + Math.sin((x + y) * 0.23) * 0.25);
    }
    g.computeVertexNormals();
    return g;
  }, []);
  const curve = useMemo(() => {
    const pts: THREE.Vector3[] = [];
    for (let i = 0; i <= 60; i++) {
      const z = 40 - i * 2.2;
      const x = Math.sin(i * 0.12) * 2.5;
      const h = Math.sin(x * 0.11) * 0.8 + Math.sin(-z * 0.07 + 1) * 1.1 + Math.sin((x - z) * 0.23) * 0.25;
      pts.push(new THREE.Vector3(x, h + 0.12, z));
    }
    return new THREE.CatmullRomCurve3(pts);
  }, []);
  const tube = useMemo(() => new THREE.TubeGeometry(curve, 300, 0.12, 12, false), [curve]);
  const snow = useMemo(() => {
    const g = new THREE.BufferGeometry();
    const a = new Float32Array(900 * 3);
    for (let i = 0; i < 900; i++) a.set([(rnd('mx' + i) - 0.5) * 40, rnd('my' + i) * 10, 30 - rnd('mz' + i) * 80], i * 3);
    g.setAttribute('position', new THREE.BufferAttribute(a, 3));
    return g;
  }, []);
  return (
    <group>
      <mesh geometry={geo} rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
        <meshStandardMaterial color="#6b97ad" roughness={1} />
      </mesh>
      <mesh geometry={tube} castShadow>
        <meshStandardMaterial color="#1a1f2e" roughness={0.5} metalness={0.3} />
      </mesh>
      {Array.from({length: 10}).map((_, i) => {
        const t = ((frame * 0.004 + i / 10) % 1);
        return (
          <mesh key={i} position={curve.getPointAt(1 - t)} scale={0.16}>
            <sphereGeometry args={[1, 12, 10]} />
            <meshBasicMaterial color={C.data} toneMapped={false} transparent opacity={pulse} />
          </mesh>
        );
      })}
      {/* rocks */}
      {Array.from({length: 30}).map((_, i) => (
        <mesh key={'r' + i} position={[(rnd('rx' + i) - 0.5) * 30, 0.2, 30 - rnd('rz' + i) * 70]} scale={0.3 + rnd('rs' + i) * 0.9}>
          <dodecahedronGeometry args={[1, 0]} />
          <meshStandardMaterial color="#4a5f6e" flatShading roughness={1} />
        </mesh>
      ))}
      <points geometry={snow} position={[0, (frame * -0.004) % 2, 0]}>
        <pointsMaterial color="#cfefff" size={2} sizeAttenuation={false} transparent opacity={0.55} />
      </points>
    </group>
  );
};
