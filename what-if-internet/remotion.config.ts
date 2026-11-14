import {Config} from '@remotion/cli/config';
import {existsSync} from 'node:fs';

// Software WebGL (SwiftShader via ANGLE) works on machines without a GPU.
// On a workstation with a GPU you can switch this to 'angle' for faster renders.
Config.setChromiumOpenGlRenderer(
  (process.env.REMOTION_GL as 'swangle' | 'angle' | 'egl' | undefined) ?? 'swangle',
);
// Use a locally installed Chrome headless shell if one is provided (or found in the
// Playwright cache). Otherwise Remotion downloads its own copy on first render.
const localShell = [
  process.env.REMOTION_BROWSER,
  '/opt/pw-browsers/chromium_headless_shell-1243/chrome-headless-shell-linux64/chrome-headless-shell',
].find((p) => p && existsSync(p));
if (localShell) {
  Config.setBrowserExecutable(localShell);
}
Config.setVideoImageFormat('jpeg');
Config.setJpegQuality(92);
Config.setCodec('h264');
Config.setCrf(18);
Config.setPixelFormat('yuv420p');
Config.setConcurrency(Number(process.env.REMOTION_CONCURRENCY ?? 4));
Config.setTimeoutInMilliseconds(120000);
