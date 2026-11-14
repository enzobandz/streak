"""Generate narration WAVs with Kokoro-82M (local, open-weight TTS) and write timing.json.

Reads   src/data/script.json
Writes  public/vo/<lineId>.wav  and  src/data/timing.json

Run from the project root:  python3 tools/tts.py [--only lineId,...]
Model files are expected in .cache/tts/ (see README for download links).
"""
import json, os, sys, hashlib, subprocess
import numpy as np
import soundfile as sf
from kokoro_onnx import Kokoro

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FPS = 30
SR = 24000

script = json.load(open(os.path.join(ROOT, "src/data/script.json")))
out_dir = os.path.join(ROOT, "public/vo")
os.makedirs(out_dir, exist_ok=True)
cache_file = os.path.join(out_dir, ".hashes.json")
hashes = json.load(open(cache_file)) if os.path.exists(cache_file) else {}

only = None
if "--only" in sys.argv:
    only = set(sys.argv[sys.argv.index("--only") + 1].split(","))

kokoro = Kokoro(os.path.join(ROOT, ".cache/tts/kokoro-v1.0.onnx"),
                os.path.join(ROOT, ".cache/tts/voices-v1.0.bin"))

def trim(samples, thresh=0.004, pad=0.06):
    idx = np.where(np.abs(samples) > thresh)[0]
    if len(idx) == 0:
        return samples
    a = max(0, idx[0] - int(pad * SR)); b = min(len(samples), idx[-1] + int(pad * SR))
    return samples[a:b]

def find_pauses(path, min_gap=0.11, thresh=0.02):
    """Frame positions (centre of each silent gap) - lets scenes sync to commas / sentence ends."""
    x, sr = sf.read(path)
    hop = int(sr / 100)
    env = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) for i in range(0, len(x) - hop, hop)])
    quiet = env < thresh * (env.max() or 1)
    out, i = [], 0
    while i < len(quiet):
        if quiet[i]:
            j = i
            while j < len(quiet) and quiet[j]:
                j += 1
            if (j - i) / 100 >= min_gap and i > 5 and j < len(quiet) - 5:
                out.append(int(round((i + j) / 2 / 100 * FPS)))
            i = j
        else:
            i += 1
    return out


timing = {"fps": FPS, "lines": {}}
for scene in script["scenes"]:
    for line in scene["lines"]:
        lid = line["id"]
        voice = line.get("voice", script["voice"])
        speed = line.get("speed", script["speed"])
        key = hashlib.sha1(f"{voice}|{speed}|{line['text']}".encode()).hexdigest()
        path = os.path.join(out_dir, f"{lid}.wav")
        need = (only is None or lid in only) and (hashes.get(lid) != key or not os.path.exists(path))
        if need:
            samples, sr = kokoro.create(line["text"], voice=voice, speed=speed, lang="en-us")
            assert sr == SR
            samples = trim(np.asarray(samples, dtype=np.float32))
            peak = np.max(np.abs(samples)) or 1.0
            samples = samples / peak * 0.89  # normalize to about -1 dBFS
            sf.write(path, samples, SR, subtype="PCM_16")
            hashes[lid] = key
            subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", path, "-c:a", "libmp3lame", "-b:a", "96k", path[:-4] + ".mp3"], check=True)
            print(f"  generated {lid}: {len(samples)/SR:.2f}s")
        info = sf.info(path)
        timing["lines"][lid] = {"seconds": round(info.duration, 3),
                                 "frames": int(np.ceil(info.duration * FPS)),
                                 "pauses": find_pauses(path)}

json.dump(hashes, open(cache_file, "w"), indent=1)
json.dump(timing, open(os.path.join(ROOT, "src/data/timing.json"), "w"), indent=1)
total = sum(v["seconds"] for v in timing["lines"].values())
print(f"Narration total: {total:.1f}s across {len(timing['lines'])} lines")
import subprocess, sys, os; subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "timeline.py")], check=True)
