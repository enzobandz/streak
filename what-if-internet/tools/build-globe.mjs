// Precompute the dotted-globe land mask from Natural Earth (public domain) via world-atlas.
// Output: src/data/globe-dots.json  -> [[lat, lon], ...] on a Fibonacci sphere.
import {readFileSync, writeFileSync} from 'node:fs';
import {feature} from 'topojson-client';
import {geoContains} from 'd3-geo';

const topo = JSON.parse(readFileSync(new URL('../node_modules/world-atlas/land-110m.json', import.meta.url)));
const land = feature(topo, topo.objects.land);
const N = 16000;
const golden = Math.PI * (3 - Math.sqrt(5));
const dots = [];
for (let i = 0; i < N; i++) {
  const y = 1 - (i / (N - 1)) * 2;
  const lat = (Math.asin(y) * 180) / Math.PI;
  const lon = ((((i * golden) * 180) / Math.PI) % 360) - 180;
  if (geoContains(land, [lon, lat])) dots.push([+lat.toFixed(2), +lon.toFixed(2)]);
}
writeFileSync(new URL('../src/data/globe-dots.json', import.meta.url), JSON.stringify(dots));
console.log(`globe: ${dots.length} land dots of ${N}`);
