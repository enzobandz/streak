import {continueRender, delayRender, staticFile} from 'remotion';

const faces: [string, string, string][] = [
  ['Space Grotesk', 'space-grotesk-latin-500-normal.woff2', '500'],
  ['Space Grotesk', 'space-grotesk-latin-700-normal.woff2', '700'],
  ['Inter', 'inter-latin-400-normal.woff2', '400'],
  ['Inter', 'inter-latin-600-normal.woff2', '600'],
  ['Inter', 'inter-latin-800-normal.woff2', '800'],
];

let loaded = false;
export const loadFonts = () => {
  if (loaded || typeof document === 'undefined') return;
  loaded = true;
  const handle = delayRender('Loading fonts');
  Promise.all(
    faces.map(([family, file, weight]) => {
      const f = new FontFace(family, `url(${staticFile('fonts/' + file)}) format('woff2')`, {weight});
      return f.load().then((ff) => (document.fonts as unknown as {add: (f: FontFace) => void}).add(ff));
    }),
  )
    .then(() => continueRender(handle))
    .catch((e) => {
      console.error(e);
      continueRender(handle);
    });
};
