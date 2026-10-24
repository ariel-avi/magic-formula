import { existsSync } from 'node:fs';

if (!existsSync(new URL('../src/wasm/mf_wasm.js', import.meta.url))) {
  console.error('web/src/wasm/mf_wasm.js is missing: run npm run wasm:build (once before: npm run wasm:setup)');
  process.exit(1);
}
