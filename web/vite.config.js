import { defineConfig } from 'vite';

// The city slice is read from the repository's processed geodata
// (assets/geo/processed), one level above this folder.
export default defineConfig({
  base: './',
  server: { fs: { allow: ['..'] } },
  build: { chunkSizeWarningLimit: 1500, assetsInlineLimit: 0 },
});
