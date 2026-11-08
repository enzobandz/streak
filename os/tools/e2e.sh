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
# Modern PC: UEFI + q35 + SATA + USB-only input, reboot, persistence
OVMF=""
for f in /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd; do [ -f "$f" ] && OVMF=$f && break; done
if [ -n "$OVMF" ]; then
    truncate -s 32M "$OUT/sata.img"
    USB="-machine q35,i8042=off -bios $OVMF -drive file=$OUT/sata.img,format=raw,if=none,id=d0 -device ide-hd,drive=d0,bus=ide.1 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,port=1 -device usb-tablet,bus=xhci.0,port=2"
    QEMU_EXTRA="$USB" python3 tools/qemu_test.py "$ISO" "$OUT/usb1" tools/e2e_usb.txt
    L="$OUT/usb1/serial.log"
    if grep -q EXCEPTION "$L"; then cat "$L"; echo "FAIL: exception (USB run)"; exit 1; fi
    for want in "storage: AHCI port" "storage: formatted blank disk" "usb: keyboard on port" "usb: tablet on port" "usb: hub" "usb: mouse on port" "removed"; do
        grep -q "$want" "$L" || { cat "$L"; echo "FAIL: USB run missing '$want'"; exit 1; }
    done
    printf 'wait 8000\ntype cat usb.txt\nkey ret\nwait 500\nshot usb_persisted\ntype shutdown\nkey ret\nwait 3000\n' > "$OUT/usb2.txt"
    QEMU_EXTRA="$USB" python3 tools/qemu_test.py "$ISO" "$OUT/usb2" "$OUT/usb2.txt" || true
    L="$OUT/usb2/serial.log"
    grep -q "storage: loaded" "$L" || { cat "$L"; echo "FAIL: SATA files not loaded after reboot"; exit 1; }
    grep -q "acpi: power off" "$L" || { cat "$L"; echo "FAIL: shutdown not reached (USB run)"; exit 1; }
    if grep -q EXCEPTION "$L"; then echo "FAIL: exception"; exit 1; fi
fi
echo "e2e: OK"
