#!/usr/bin/env bash
# Full-episode render in resumable chunks:
#   1. video chunks as H.264 + lossless PCM audio in Matroska (skips chunks that already
#      exist, so an interrupted render can be resumed). PCM audio concatenates without gaps.
#   2. lossless concat, then one AAC encode with loudness normalisation to -14 LUFS (YouTube)
set -euo pipefail
cd "$(dirname "$0")/.."
CHUNK=${CHUNK:-1500}
OUT=out
mkdir -p $OUT/chunks
TOTAL=$(node -e "console.log(require('./src/data/timeline.json').durationInFrames)")
echo "Episode: $TOTAL frames"

: > $OUT/chunks/list.txt
for ((s=0; s<TOTAL; s+=CHUNK)); do
  e=$(( s + CHUNK - 1 )); (( e >= TOTAL )) && e=$(( TOTAL - 1 ))
  f=$(printf "chunk-%05d.mkv" $s)
  if [ ! -f $OUT/chunks/$f ]; then
    echo "[$(date +%T)] rendering frames $s-$e"
    npx remotion render Episode $OUT/chunks/tmp-$f --codec=h264-mkv --frames=$s-$e --log=error
    mv $OUT/chunks/tmp-$f $OUT/chunks/$f
  fi
  echo "file '$f'" >> $OUT/chunks/list.txt
done

ffmpeg -loglevel error -y -f concat -safe 0 -i $OUT/chunks/list.txt -c copy $OUT/episode-master.mkv
ffmpeg -loglevel error -y -i $OUT/episode-master.mkv -map 0:v -map 0:a -c:v copy \
  -af "loudnorm=I=-14:TP=-1.5:LRA=11" -c:a aac -b:a 256k -ar 48000 \
  -movflags +faststart $OUT/what-if-internet-24h.mp4
echo "[$(date +%T)] DONE -> $OUT/what-if-internet-24h.mp4"
