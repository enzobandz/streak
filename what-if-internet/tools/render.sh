#!/usr/bin/env bash
# Full-episode render in resumable chunks:
#   1. audio-only render of the whole timeline (narration + score + SFX)
#   2. muted video chunks (skips chunks that already exist, so it can be resumed)
#   3. lossless concat + mux + loudness normalisation to -14 LUFS (YouTube target)
set -euo pipefail
cd "$(dirname "$0")/.."
CHUNK=${CHUNK:-1500}
OUT=out
mkdir -p $OUT/chunks
TOTAL=$(node -e "console.log(require('./src/data/timeline.json').durationInFrames)")
echo "Episode: $TOTAL frames"

[ -f $OUT/episode-audio.wav ] || npx remotion render Episode $OUT/episode-audio.wav --codec=wav --log=error

: > $OUT/chunks/list.txt
for ((s=0; s<TOTAL; s+=CHUNK)); do
  e=$(( s + CHUNK - 1 )); (( e >= TOTAL )) && e=$(( TOTAL - 1 ))
  f=$(printf "chunk-%05d.mp4" $s)
  if [ ! -f $OUT/chunks/$f ]; then
    echo "[$(date +%T)] rendering frames $s-$e"
    npx remotion render Episode $OUT/chunks/$f.tmp.mp4 --frames=$s-$e --muted --log=error
    mv $OUT/chunks/$f.tmp.mp4 $OUT/chunks/$f
  fi
  echo "file '$f'" >> $OUT/chunks/list.txt
done

ffmpeg -loglevel error -y -f concat -safe 0 -i $OUT/chunks/list.txt -c copy $OUT/video-only.mp4
ffmpeg -loglevel error -y -i $OUT/video-only.mp4 -i $OUT/episode-audio.wav \
  -map 0:v -map 1:a -c:v copy -af "loudnorm=I=-14:TP=-1.5:LRA=11" -c:a aac -b:a 256k -ar 48000 \
  -movflags +faststart -shortest $OUT/what-if-internet-24h.mp4
echo "[$(date +%T)] DONE -> $OUT/what-if-internet-24h.mp4"
