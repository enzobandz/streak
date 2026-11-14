"""Write SCRIPT.md (narration + on-screen sources + timecodes) from the timeline."""
import json, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tl = json.load(open(os.path.join(ROOT, "src/data/timeline.json")))
fps = tl["fps"]
tc = lambda fr: f"{int(fr / fps // 60)}:{fr / fps % 60:04.1f}"
out = ["# Narration script — *What If the Entire Internet Shut Down for 24 Hours?*", "",
       "_Generated from `src/data/timeline.json` by `tools/export-script.py`. Edit the words in `src/data/script.json`._", "",
       f"Runtime: **{tc(tl['durationInFrames'])}** · {sum(len(s['lines']) for s in tl['scenes'])} narration lines · voice: Kokoro-82M `af_heart`", ""]
words = 0
for s in tl["scenes"]:
    out.append(f"## {tc(s['from'])} — {s['chapter'] or s['id']}")
    out.append("")
    if not s["lines"]:
        out.append("_(title sequence — music and sound design only)_\n")
    for l in s["lines"]:
        words += len(l["text"].split())
        out.append(f"**{tc(s['from'] + l['from'])}** {l['text']}")
        if l.get("source"):
            out.append(f"  \n> On-screen source: {l['source']}")
        out.append("")
out.insert(5, f"Word count: {words}\n")
open(os.path.join(ROOT, "SCRIPT.md"), "w").write("\n".join(out))
print("SCRIPT.md written,", words, "words")
