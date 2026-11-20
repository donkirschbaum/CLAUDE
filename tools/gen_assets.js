// Erzeugt die Werks-Samples (WAV) und die Werks-Presets (JSON) aus der Web-Version,
// damit Plugin und Browser-Version exakt gleich klingen.
// Aufruf: node tools/gen_assets.js
const fs = require('fs'), path = require('path'), vm = require('vm');
const root = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'web', 'index.html'), 'utf8');
const script = html.split('<script>')[1].split('</script>')[0];
const a = script.indexOf('/* ============ Eingebaute Samples');
const b = script.indexOf('/* ============ Sample-Library');
if (a < 0 || b < 0) throw new Error('Marker in web/index.html nicht gefunden');
const code = script.slice(a, b) + '\n;globalThis.__out={SOURCES,PRESETS,GSR};';
const ctx = { Math, Float32Array, Float64Array, Object, Array, String, Number, Set, console };
vm.createContext(ctx);
vm.runInContext(code, ctx);
const { SOURCES, PRESETS, GSR } = ctx.__out;

const outDir = path.join(root, 'assets', 'samples');
fs.mkdirSync(outDir, { recursive: true });
function writeWav(file, data, sr) {
  const n = data.length, buf = Buffer.alloc(44 + n * 2);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write('WAVE', 8);
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22);
  buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 2, 28); buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34);
  buf.write('data', 36); buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) buf.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(data[i] * 32767))), 44 + i * 2);
  fs.writeFileSync(file, buf);
}
for (const id of Object.keys(SOURCES)) {
  writeWav(path.join(outDir, `s_${id}.wav`), SOURCES[id](), GSR);
}
const presets = PRESETS.map(p => ({ cat: p.cat, name: p.name, src: p.src, p: p.p }));
fs.writeFileSync(path.join(root, 'assets', 'presets.json'), JSON.stringify(presets));
console.log(`${Object.keys(SOURCES).length} Samples und ${presets.length} Presets erzeugt.`);
