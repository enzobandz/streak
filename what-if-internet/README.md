# What If Reality — *What If the Entire Internet Shut Down for 24 Hours?*

An 8-minute (7:58), 1920×1080, 30 fps animated documentary built entirely in code with
**Remotion 4 + React 18 + Three.js (react-three-fiber)**. Narration, music and sound effects are
generated locally by scripts in this folder; nothing comes from stock libraries.

| Deliverable | Where |
|---|---|
| Full episode (MP4, H.264 + AAC, −14 LUFS) | `out/what-if-internet-24h.mp4` (rendered by `npm run render:full`, not committed — too large for git) |
| 30-second proof of concept (cold open + title) | `out/proof-of-concept.mp4` (`npm run render:poc`) |
| Subtitles for YouTube | `out/what-if-internet-24h.srt` (generated with the timeline) |
| Narration script with sources & timecodes | [`SCRIPT.md`](SCRIPT.md) |
| Shot-by-shot storyboard | [`STORYBOARD.md`](STORYBOARD.md) |
| Fact-check notes | [Fact-checking](#fact-checking) below |

---

## Quick start

```bash
cd what-if-internet
npm install
npm run studio          # Remotion Studio: scrub, preview and tweak every scene live
```

The repository already contains the generated narration (`public/vo/*.mp3`), score
(`public/music/score.mp3`), sound effects (`public/sfx/*.wav`), timing data and globe data,
so the Studio and renders work right after `npm install`.

Render:

```bash
npm run render:poc      # 30 s proof of concept  -> out/proof-of-concept.mp4
npm run render:full     # full episode in resumable chunks -> out/what-if-internet-24h.mp4
npm run render          # full episode in a single Remotion pass (simplest, no resume)
npx remotion render Scene-money out/money.mp4   # any single chapter
```

`remotion.config.ts` uses **software WebGL** (`swangle`) so it works on servers without a GPU.
On a machine with a GPU set `REMOTION_GL=angle` (or `egl` on Linux) for much faster renders.
Reference speed on 4 CPU cores, no GPU: ≈0.7 s per frame, ≈3 hours for the full episode.
`REMOTION_CONCURRENCY` sets the number of parallel browser tabs; `REMOTION_BROWSER` points to a
specific Chrome headless-shell binary (otherwise Remotion downloads one, or a Playwright copy is
auto-detected).

## Architecture

```
src/data/script.json ──► tools/tts.py ──► public/vo/<line>.mp3 + timing.json (durations, pauses)
                                   │
                                   ▼
                       tools/timeline.py ──► src/data/timeline.json  (single source of truth:
                                   │                                   absolute frame of every scene,
                                   │                                   line and natural pause)
              ┌────────────────────┼──────────────────────────┐
              ▼                    ▼                          ▼
     tools/audio.py        Remotion (src/…)          out/…srt subtitles
     score.mp3 (ducked     scenes read timing via
     under narration)      useScene()/useLines()
     + sfx/*.wav
```

* **Narration drives everything.** Each scene's length = its lead-in + narration lines + gaps + tail.
  Re-write a sentence, run `npm run assets`, and the scene, the shot cuts inside it, the score's
  mood changes, the ducking and the subtitles all re-time automatically.
* **Word-level sync without word timestamps.** `tts.py` detects the natural pauses (commas,
  full stops) in every clip. Scenes key visual beats to them, e.g. the countdown in the cold open
  hits 00:00:00 on `L.c3.p[last]` — the pause right after "simply stopped".
* **Deterministic.** No `Math.random()`, no clock time, no `useFrame` loops: every position is a pure
  function of the frame number (`remotion`'s seeded `random()`, keyframed cameras, `integrate()` for
  variable-speed motion). Any frame can be rendered in isolation, in any order, on any machine.

### Folder map

| Path | What it is |
|---|---|
| `src/Root.tsx` | Compositions: `Episode`, `ProofOfConcept`, and `Scene-<id>` for every chapter |
| `src/Episode.tsx` | Assembles scenes, transitions (wipe + whoosh), narration, score, chapter cards, clock HUD, source tags, film finish |
| `src/scenes/*.tsx` | One file per chapter; each is split into shots (`<Sequence>`) cut on narration lines |
| `src/components/three/Stage.tsx` | `Stage` (ThreeCanvas wrapper with fog/background) · `CameraRig` (keyframed camera + handheld drift) · `Lights` (house lighting rig) |
| `src/components/three/Globe.tsx` | Dotted Earth from Natural Earth land data, fresnel atmosphere, illustrative cable arcs with data pulses, markers, `connectivity` 0–1 |
| `src/components/three/Person.tsx` | The house character: walk cycle, phone pose, head turns, moods (smile / neutral / worried), blinking, props; `lookFor(seed)` gives varied stable outfits |
| `src/components/three/City.tsx` | Procedural buildings with day/night window textures, `Street`, `Skyline`, cars, trees, lamps |
| `src/components/three/Props.tsx` | Router, card terminal, ATM, pylon, broadcast mast, server rack, desk, robot arm, plane, tower, ship, crane, truck, satellite, hospital, water plant, big switch, volcanic island |
| `src/components/three/Network.tsx` | Network-of-networks graph (islands / exchange / BGP route) and the seabed cable |
| `src/components/ui/*` | Motion graphics: chapter cards, clock HUD, stats, icon cards, tags, bubbles, departure board, phone map UI, logo, original SVG icon set |
| `src/audio/Sfx.tsx` | `<Sfx name at volume>` — place an effect at a frame inside any shot |
| `tools/tts.py` | Narration with Kokoro-82M (local, open weights, Apache-2.0) + pause detection |
| `tools/timeline.py` | Builds `timeline.json` and the `.srt` file |
| `tools/audio.py` | Synthesises 23 sound effects and the full-length score (numpy/scipy, seeded) |
| `tools/build-globe.mjs` | Land mask for the dotted globe from `world-atlas` (Natural Earth, public domain) |
| `tools/render.sh` | Chunked, resumable full render → concat → AAC + loudness normalisation |
| `tools/export-script.py` | Regenerates `SCRIPT.md` from the timeline |

## Editing guide

**Change the words.** Edit `src/data/script.json` (each line has an `id`, `text`, optional
`source` that appears on screen). Then regenerate the derived assets:

```bash
pip install kokoro-onnx soundfile numpy scipy           # once
mkdir -p .cache/tts && cd .cache/tts                    # once: model files (~350 MB)
curl -LO https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx
curl -LO https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/voices-v1.0.bin
cd ../..
npm run assets      # globe data → narration (only changed lines are re-voiced) → timeline → score
python3 tools/export-script.py
```

* Change voice or pace with `"voice"` / `"speed"` at the top of `script.json` (or per line).
  Other Kokoro voices: `am_michael`, `bf_emma`, `bm_george`, … To use a human recording instead,
  drop `public/vo/<lineId>.wav` + `.mp3` files in place and run `python3 tools/timeline.py`.
* Pacing: `LINE_GAP` in `tools/timeline.py`, and `leadIn` / `tail` / `minFrames` per scene.

**Change the visuals.** Open `npm run studio`, pick `Scene-<id>`, and edit the matching file in
`src/scenes/`. Inside a scene, `const L = useLines()` gives `L.<lineId>.from`, `.end` and `.p[n]`
(pause frames), all relative to the scene start — key every event to those, never to hard-coded
frames, so timing survives script edits. Colours and fonts live in `src/theme.ts`.

**Change the music.** `tools/audio.py` → `MOODS` maps each scene id to layer levels
(`pad`, `arp`, `bass`, `kick`, `hat`, `piano`, `drone`); key, tempo and chords are at the top
of the music section. Re-run `python3 tools/audio.py`.

## Toolchain and honest notes on limitations

| Need | Used | Notes |
|---|---|---|
| Animation & render | Remotion 4.0.543, React 18, three r169, @react-three/fiber 8 | Pinned exact versions |
| Narration | Kokoro-82M via `kokoro-onnx` (local) | A good open-weight synthetic voice, but not a human narrator. For a flagship release, record a human voice-over with the same line ids — the pipeline re-times around it. ElevenLabs etc. were not used (no account/keys available in this environment). |
| Music & SFX | Procedural synthesis (`tools/audio.py`) | Fully original and licence-free; simpler than a composed score. Swap `public/music/score.mp3` for a licensed or commissioned track if desired (keep it ≥ the episode length). |
| Fonts | Space Grotesk, Inter (SIL OFL, bundled in `public/fonts`) | No network fetch at render time |
| Map data | Natural Earth via `world-atlas` (public domain) | Cable routes on the globe are **illustrative great-circle arcs**, not surveyed paths |
| Rendering | Headless Chromium with SwiftShader (CPU) | Slow but works anywhere; use a GPU for speed |

## Fact-checking

The episode is framed as a **thought experiment**: no single switch can turn off the whole
internet. Claims that are projections are worded as such ("would likely", "may", "should").
Concrete figures and events, with the source shown on screen:

| Claim in narration | Source / basis |
|---|---|
| "More than five billion people use the internet" (on screen 5.5 bn, ~68%) | ITU *Facts and Figures 2024* |
| "More than seventy thousand independent systems" | Active autonomous systems in the global BGP table ≈ 75–78k (CIDR Report, APNIC, 2024) |
| Exchange points + BGP as the internet's routing "map" | Standard description of inter-domain routing |
| Most intercontinental traffic uses submarine cables; deep-sea cables ≈ garden-hose thick | TeleGeography (≈ 570 active & planned systems in 2024); TeleGeography FAQ on cable diameter |
| GPS works without the internet | GPS satellites broadcast one-way signals; receivers need no data connection (map tiles do) |
| Card terminals can store transactions offline up to limits | Offline / deferred authorisation with floor limits is a standard terminal feature (varies by merchant and issuer) |
| World economy ≈ $110 trillion a year → ≈ $300 billion a day | IMF World Economic Outlook, nominal world GDP 2024; 110 ÷ 365 ≈ 0.30 trillion |
| July 2024: faulty CrowdStrike update crashed ≈ 8.5 million Windows devices; thousands of flights canceled | Microsoft blog, 20 July 2024; widely reported airline cancellations |
| Power grids largely controlled over dedicated networks | Grid SCADA / operational-technology networks are separated from the public internet by design and regulation (e.g. NERC CIP); the script says "should stay on" |
| Hospital downtime procedures | Standard EHR downtime planning (paper charts, printed medication administration records, runners) |
| Egypt cut off for about five days in 2011 | 27/28 Jan – 2 Feb 2011 |
| Tonga's only international cable severed by the January 2022 eruption; about five weeks to restore | Hunga Tonga–Hunga Haʻapai eruption 15 Jan 2022; cable repaired 22 Feb 2022 |
| Access Now documents hundreds of shutdowns a year (on screen: 296 in 54 countries, 2024) | Access Now #KeepItOn 2024 report |
| Facebook's ~6-hour outage in October 2021; engineers warned about power swings when restoring and brought services back gradually | Meta Engineering blog, 5 Oct 2021 ("More details about the October 4 outage") |
| "Thundering herd" | Standard engineering term for many clients retrying at once; the load chart is labelled illustrative |

Time zones in the cold open: 00:00 UTC = 09:00 in Tokyo (UTC+9) and 19:00 in New York in winter
(EST, UTC−5; 20:00 in summer — the narration says "early evening").
