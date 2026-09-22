import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateSync } from 'node:zlib';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const source = await readFile(resolve(root, 'assets/geoid/egm2008-5.pgm'));
const output = resolve(process.argv[2]);
const header = [];
let position = 0, offset, scale;
while (header.length < 3) {
    const end = source.indexOf(10, position);
    if (end < 0) throw new Error('Incomplete geoid PGM header');
    const line = source.subarray(position, end).toString('ascii').trim();
    position = end + 1;
    if (line.startsWith('# Offset ')) offset = Number(line.slice(9));
    if (line.startsWith('# Scale ')) scale = Number(line.slice(8));
    if (line && !line.startsWith('#')) header.push(line);
}
const [width, height] = header[1].split(/\s+/).map(Number);
const count = width * height;
if (header[0] !== 'P5' || header[2] !== '65535' || !Number.isFinite(offset) || !Number.isFinite(scale)
    || !Number.isInteger(count) || count <= 0 || source.length - position !== count * 2)
    throw new Error('Unsupported geoid PGM');

const encoded = Buffer.alloc(28 + count * 2);
encoded.write('AGD1');
encoded.writeUInt32BE(width, 4);
encoded.writeUInt32BE(height, 8);
encoded.writeDoubleBE(offset, 12);
encoded.writeDoubleBE(scale, 20);
// Split each row's 16-bit horizontal differences into high and low byte planes.
for (let y = 0; y < height; ++y) {
    let previous = 0;
    for (let x = 0; x < width; ++x) {
        const index = y * width + x;
        const value = source.readUInt16BE(position + index * 2);
        const delta = (value - previous) & 65535;
        previous = value;
        encoded[28 + index] = delta >>> 8;
        encoded[28 + count + index] = delta & 255;
    }
}
const size = Buffer.alloc(4);
size.writeUInt32BE(encoded.length);
await mkdir(dirname(output), { recursive: true });
await writeFile(output, Buffer.concat([size, deflateSync(encoded, { level: 9 })]));
