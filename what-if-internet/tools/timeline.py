"""Build the episode timeline (absolute frame positions) from script.json + timing.json.

Single source of truth for scene/line timing, consumed by Remotion (src/data/timeline.json),
by the music generator (tools/audio.py) and by the subtitle exporter.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FPS = 30
LINE_GAP = 26          # frames of breathing room between narration lines


def build():
    script = json.load(open(os.path.join(ROOT, "src/data/script.json")))
    timing = json.load(open(os.path.join(ROOT, "src/data/timing.json")))
    cursor = 0
    scenes = []
    for sc in script["scenes"]:
        start = cursor
        local = sc.get("leadIn", 0)
        lines = []
        for i, ln in enumerate(sc["lines"]):
            dur = timing["lines"][ln["id"]]["frames"]
            lines.append({"id": ln["id"], "from": local, "duration": dur, "text": ln["text"],
                          "source": ln.get("source"),
                          "pauses": timing["lines"][ln["id"]].get("pauses", [])})
            local += dur + (LINE_GAP if i < len(sc["lines"]) - 1 else 0)
        local += sc.get("tail", 0)
        local = max(local, sc.get("minFrames", 0))
        scenes.append({"id": sc["id"], "chapter": sc.get("chapter"), "clock": sc.get("clock"),
                       "from": start, "duration": local, "lines": lines})
        cursor += local
    tl = {"fps": FPS, "width": 1920, "height": 1080, "durationInFrames": cursor, "scenes": scenes}
    json.dump(tl, open(os.path.join(ROOT, "src/data/timeline.json"), "w"), indent=1, ensure_ascii=False)
    return tl


def fmt(sec):
    h, rem = divmod(sec, 3600); m, s = divmod(rem, 60)
    return f"{int(h):02d}:{int(m):02d}:{int(s):02d},{int(round((s - int(s)) * 1000)):03d}"


def write_srt(tl):
    out, n = [], 1
    for sc in tl["scenes"]:
        for ln in sc["lines"]:
            a = (sc["from"] + ln["from"]) / FPS; b = a + ln["duration"] / FPS
            out.append(f"{n}\n{fmt(a)} --> {fmt(b)}\n{ln['text']}\n"); n += 1
    os.makedirs(os.path.join(ROOT, "out"), exist_ok=True)
    open(os.path.join(ROOT, "out/what-if-internet-24h.srt"), "w").write("\n".join(out))


if __name__ == "__main__":
    tl = build(); write_srt(tl)
    for s in tl["scenes"]:
        print(f"{s['id']:<14} {s['from']/FPS:7.1f}s  len {s['duration']/FPS:5.1f}s")
    print(f"TOTAL {tl['durationInFrames']} frames = {tl['durationInFrames']/FPS/60:.2f} min")
