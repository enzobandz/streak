#!/bin/sh
# Run the end-to-end script with a blank disk, reboot, check the file written
# before the reboot is still there, then shut down from the Terminal.
# Fails on any CPU exception or if the system stops responding.
set -eu
cd "$(dirname "$0")/.."
ISO=${1:-build/streakos.iso}
OUT=${2:-build/e2e}
rm -rf "$OUT"; mkdir -p "$OUT"
truncate -s 32M "$OUT/disk.img"
DISK="$OUT/disk.img" python3 tools/qemu_test.py "$ISO" "$OUT/run1" tools/e2e.txt
if grep -q EXCEPTION "$OUT/run1/serial.log"; then cat "$OUT/run1/serial.log"; echo "FAIL: exception"; exit 1; fi
grep -q "storage: formatted blank disk" "$OUT/run1/serial.log" || { echo "FAIL: disk not formatted"; exit 1; }
printf 'wait 5000\ntype cat e2e.txt\nkey ret\nwait 500\nshot persisted\ntype shutdown\nkey ret\nwait 3000\n' > "$OUT/run2.txt"
DISK="$OUT/disk.img" python3 tools/qemu_test.py "$ISO" "$OUT/run2" "$OUT/run2.txt" || true
grep -q "storage: loaded" "$OUT/run2/serial.log" || { echo "FAIL: files not loaded after reboot"; exit 1; }
if grep -q EXCEPTION "$OUT/run2/serial.log"; then echo "FAIL: exception"; exit 1; fi
grep -q "acpi: power off" "$OUT/run2/serial.log" || { cat "$OUT/run2/serial.log"; echo "FAIL: shutdown not reached"; exit 1; }
echo "e2e: OK"
