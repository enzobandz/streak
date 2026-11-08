#!/bin/sh
# Boot the ISO headless (BIOS, and UEFI if OVMF is installed) and check the
# kernel reaches the desktop. Used by CI.
set -eu
ISO=${1:-build/fluvia.iso}
OUT=$(mktemp -d)
boot() {
    name=$1; shift
    timeout 60 qemu-system-x86_64 "$@" -cdrom "$ISO" -m 256M -display none \
        -serial "file:$OUT/$name.log" -no-reboot &
    pid=$!
    for _ in $(seq 1 50); do
        if grep -q "desktop ready" "$OUT/$name.log" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            echo "$name: OK"
            cat "$OUT/$name.log"
            return 0
        fi
        sleep 1
    done
    kill "$pid" 2>/dev/null || true
    echo "$name: did not reach the desktop"; cat "$OUT/$name.log"
    return 1
}
boot bios
for fw in /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd; do
    if [ -f "$fw" ]; then boot uefi -bios "$fw"; break; fi
done
