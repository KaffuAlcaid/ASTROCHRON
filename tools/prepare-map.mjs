import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { unzipSync } from 'fflate';
import mapshaper from 'mapshaper';
import earcut, { flatten, deviation } from 'earcut';
import { geoArea, geoEquirectangular } from 'd3-geo';
import { geoProject } from 'd3-geo-projection';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const sourceDirectory = resolve(root, 'assets/natural-earth/source');
const output = resolve(process.argv[2] ?? resolve(root, '.cache/generated/world-map.json'));
const projection = geoEquirectangular().scale(180 / Math.PI).translate([0, 0]).precision(0);
const layers = {
  land: 'ne_50m_land',
  lakes: 'ne_50m_lakes',
  borders: 'ne_50m_admin_0_boundary_lines_land',
  cities: 'ne_50m_populated_places',
};
const versions = {};
const inputs = {};
const decode = bytes => new TextDecoder('utf-8', { fatal: true }).decode(bytes);
const rounded = value => Math.round(value * 100000) / 100000;

for (const [key, name] of Object.entries(layers)) {
  const files = unzipSync(await readFile(resolve(sourceDirectory, `${name}.zip`)));
  for (const extension of ['shp', 'shx', 'dbf', 'prj', 'cpg']) {
    if (!files[`${name}.${extension}`]) {
      throw new Error(`${name}.zip: missing ${extension} file`);
    }
  }
  if (!decode(files[`${name}.prj`]).includes('WGS_1984')) {
    throw new Error(`${name}: expected WGS84 coordinates`);
  }
  if (decode(files[`${name}.cpg`]).trim().toUpperCase() !== 'UTF-8') {
    throw new Error(`${name}: expected UTF-8 attributes`);
  }
  versions[key] = decode(files[`${name}.VERSION.txt`]).trim();
  inputs[key] = Object.fromEntries(Object.entries(files).map(([file, bytes]) => [file, Buffer.from(bytes)]));
}

async function readLayer(key, interval) {
  const simplify = interval ? ` -simplify weighted interval=${interval} keep-shapes` : '';
  const result = await mapshaper.applyCommands(
    `-i ${layers[key]}.shp encoding=utf8${simplify} -o output.geojson format=geojson`,
    inputs[key],
  );
  return JSON.parse(result['output.geojson']);
}

function projectPolygon(coordinates) {
  // D3's spherical polygon winding is the reverse of RFC 7946 exterior rings.
  if (geoArea({ type: 'Polygon', coordinates }) > 2 * Math.PI) {
    coordinates = coordinates.map(ring => [...ring].reverse());
  }
  const geometry = geoProject({ type: 'Polygon', coordinates }, projection);
  if (!geometry) return [];
  return geometry.type === 'Polygon' ? [geometry.coordinates] : geometry.coordinates;
}

function makeMesh(collection) {
  const vertices = [];
  const indices = [];
  for (const feature of collection.features) {
    if (!feature.geometry) continue;
    const { type, coordinates } = feature.geometry;
    if (type !== 'Polygon' && type !== 'MultiPolygon') {
      throw new Error(`Expected polygons, received ${type}`);
    }
    const polygons = type === 'Polygon' ? [coordinates] : coordinates;
    for (const polygon of polygons) {
      for (const piece of projectPolygon(polygon)) {
        const flat = flatten(piece);
        const triangles = earcut(flat.vertices, flat.holes, flat.dimensions);
        const areaError = deviation(flat.vertices, flat.holes, flat.dimensions, triangles);
        if (areaError > 0.001) {
          throw new Error(`Polygon triangulation area mismatch: ${areaError}`);
        }
        const offset = vertices.length / 2;
        vertices.push(...flat.vertices.map(rounded));
        for (const index of triangles) indices.push(index + offset);
      }
    }
  }
  return { vertices, indices };
}

function makeLines(collection) {
  const result = [];
  for (const feature of collection.features) {
    const geometry = geoProject(feature.geometry, projection);
    if (!geometry) continue;
    const lines = geometry.type === 'LineString' ? [geometry.coordinates] : geometry.coordinates;
    for (const line of lines) {
      result.push(line.flatMap(point => point.map(rounded)));
    }
  }
  return result;
}

const detailLevels = [];
for (const interval of [15000, 2500]) {
  const land = makeMesh(await readLayer('land', interval));
  const lakes = makeMesh(await readLayer('lakes', interval));
  const borders = makeLines(await readLayer('borders', interval));
  detailLevels.push({ land, lakes, borders });
  console.log(`Map detail ${interval} m: ${land.vertices.length / 2} land vertices, ${lakes.vertices.length / 2} lake vertices`);
}

const cityFeatures = await readLayer('cities');
const cities = cityFeatures.features.map(({ geometry, properties: p }) => ({
  name: p.NAME_ZH || p.NAME,
  originalName: p.NAME,
  longitude: rounded(geometry.coordinates[0]),
  latitude: rounded(geometry.coordinates[1]),
  rank: p.SCALERANK,
  population: p.POP_MAX,
  timeZone: p.TIMEZONE || '',
})).sort((a, b) => a.rank - b.rank || b.population - a.population);

const data = { projection: 'plate-carree', versions, detailLevels, cities };
await mkdir(dirname(output), { recursive: true });
await writeFile(output, JSON.stringify(data), 'utf8');
console.log(`Prepared ${cities.length} cities: ${output}`);
