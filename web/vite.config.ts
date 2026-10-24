import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'vitest/config';

const webDir = dirname(fileURLToPath(import.meta.url));
// The repository root: the example .tir files are imported with `?raw` from tests/data.
const repoRoot = resolve(webDir, '..');

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: { '@': resolve(webDir, 'src') },
  },
  server: {
    host: 'localhost',
    port: 5173,
    strictPort: true,
    fs: { allow: [webDir, resolve(repoRoot, 'tests/data')] },
  },
  test: {
    projects: [
      {
        extends: true,
        test: {
          name: 'unit',
          include: ['src/**/*.test.ts'],
          exclude: ['src/**/*.smoke.test.ts', 'e2e/**'],
          environment: 'node',
        },
      },
      {
        extends: true,
        test: {
          name: 'smoke',
          include: ['src/**/*.smoke.test.ts'],
          environment: 'node',
          testTimeout: 30000,
        },
      },
    ],
    coverage: {
      provider: 'v8',
      include: ['src/domain/**/*.ts'],
      exclude: ['**/*.test.ts', 'src/domain/model.ts'],
      reporter: ['text', 'html'],
      reportsDirectory: 'coverage',
      thresholds: { lines: 100, branches: 100, functions: 100, statements: 100 },
    },
  },
});
