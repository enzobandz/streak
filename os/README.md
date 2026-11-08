# StreakOS

**A from-scratch x86 operating system with a fluid, compositing desktop.**
It boots from an ISO on a PC or in a virtual machine on Windows, with no Linux
underneath. The kernel, drivers, window manager, renderer and apps are all in
this folder and were written for StreakOS.

![StreakOS desktop](docs/desktop.jpg)

| Start menu with live search | Aero-style snapping | Light mode |
|---|---|---|
| ![Start](docs/start-menu.jpg) | ![Snap](docs/snap.jpg) | ![Light](docs/light-mode.jpg) |

## What makes it fluid

* **Compositing window manager.** Every window renders into its own surface.
  A compositor redraws only damaged rectangles of a back buffer and copies them
  to the screen, frame-paced at about 60 fps on a 1000 Hz timer. Under
  QEMU's *software* CPU emulation it holds 55 to 62 fps.
* **Time-based animations with easing:** window open/close (scale + fade),
  minimise into its taskbar icon and restore out of it, maximise/snap morphs,
  a Start menu that slides up, toasts that spring in, taskbar hover fades, and
  stretchy running indicators. Settings → Animations switches between Normal,
  Relaxed (slower) and Off.
* **Modern rendering:** anti-aliased text (Inter and JetBrains Mono,
  pre-rasterised), anti-aliased rounded corners, soft drop shadows, and
  "acrylic" blur behind the taskbar, Start menu and notifications. The
  wallpaper is generated procedurally at boot.
* **Window management:** drag, resize from any edge, double-click the title
  bar to maximise, drag to the top or sides to snap (with an animated
  preview), and drag a maximised window down to restore it under the cursor.
* **Seamless mouse in VMs.** StreakOS speaks the VMware/QEMU/VirtualBox
  absolute-pointer protocol, so the cursor follows your Windows mouse without
  being captured. It falls back to a normal PS/2 mouse with acceleration
  and scroll wheel.

## Apps

| App | What it does |
|---|---|
| **Terminal** | Shell with history, Tab completion and ~25 commands (`help`, `ls`, `cat`, `write`, `edit`, `open`, `ps`, `kill`, `calc`, `neofetch`, `theme`, `wallpaper`, `reboot`, `shutdown`, ...) |
| **Files** | Browse, create and delete files; double-click opens them in Notes |
| **Notes** | Text editor with caret navigation, auto-indent, Ctrl+S, autosave on close |
| **Paint** | Smooth anti-aliased brush, 10 colours, 3 sizes |
| **Calculator** | Mouse and keyboard input, animated keys, chained operations |
| **System Monitor** | Live frame-time graph, FPS, memory, CPU model, uptime |
| **Settings** | Dark/light mode, 6 accent colours, 4 wallpapers, animation speed |
| **Snake** | Arcade game with smoothly interpolated movement |

### Keyboard shortcuts

| Keys | Action |
|---|---|
| <kbd>Win</kbd> | Open/close Start (then type to search, <kbd>Enter</kbd> to launch) |
| <kbd>Alt</kbd>+<kbd>Tab</kbd> | Cycle windows |
| <kbd>Alt</kbd>+<kbd>F4</kbd> | Close window |
| <kbd>Alt</kbd>+<kbd>↑</kbd> / <kbd>←</kbd> / <kbd>→</kbd> | Maximise / snap left / snap right |
| <kbd>Alt</kbd>+<kbd>↓</kbd> | Restore, or minimise |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>T</kbd> | New Terminal |

## Run it on Windows

Get `streakos.iso`: download the **streakos-iso** artifact from the
**StreakOS** GitHub Actions workflow, or [build it](#build-it).

### VirtualBox (easiest)
1. *New* → Type **Other**, Version **Other/Unknown**, 256 MB RAM or more, no disk needed.
2. *Settings → Display*: Graphics Controller **VBoxVGA** or **VMSVGA**, 16 MB+ video memory.
3. *Settings → Storage*: attach `streakos.iso` to the optical drive. Start.

### QEMU for Windows
```powershell
qemu-system-x86_64 -cdrom streakos.iso -m 256M -vga std
# add  -accel whpx  for hardware acceleration (requires Windows Hypervisor Platform)
```

### Hyper-V
Use a **Generation 1** VM (it emulates the PS/2 keyboard and mouse StreakOS
drives) and boot the ISO from its DVD drive.

### Real hardware (USB stick)
Write the ISO with [Rufus](https://rufus.ie) in **DD image** mode, disable
Secure Boot, and boot from USB. The ISO boots in both UEFI and legacy BIOS mode.
StreakOS needs PS/2 input or the firmware's USB legacy emulation for keyboard
and mouse. Many laptops with only USB input won't respond yet (see
limitations).

## Build it

On Linux, or in **WSL** on Windows (`wsl --install -d Ubuntu`):

```bash
sudo apt install build-essential nasm grub-pc-bin grub-efi-amd64-bin grub-common xorriso mtools qemu-system-x86
make -C os          # -> os/build/streakos.iso
make -C os run      # boot it in QEMU
os/tools/smoke_test.sh os/build/streakos.iso   # headless BIOS + UEFI boot check
```

## How it's built

```
boot/boot.asm     Multiboot2 header (asks GRUB for a linear framebuffer), stack, FPU, -> kmain
boot/isr.asm      interrupt stubs, GDT/IDT loading
kernel/main.c     boot: memory map, hardware init, splash, 60 fps frame loop
kernel/cpu.c      GDT, IDT, 8259 PIC, PIT timer (1 kHz), CMOS clock, CPUID, reboot/power-off
kernel/input.c    PS/2 keyboard (scan set 1), PS/2 + IntelliMouse wheel, vmmouse absolute pointer
kernel/mem.c      first-fit heap with coalescing
kernel/gfx.c      rasteriser: AA shapes, blending, rounded/scaled blits, shadows, text, blur
kernel/wm.c       compositor, damage tracking, window lifecycle and animations, input routing
kernel/shell.c    theme, wallpaper generator, icons, taskbar, Start menu, toasts
kernel/anim.c     easing curves (cubic, quint, back, spring)
kernel/fs.c       in-memory file system
apps/*.c          the apps; each is an app_t with init/paint/key/mouse/tick/close callbacks
tools/genfont.py  rasterises the fonts into kernel/fontdata.h (output is committed)
```

Apps draw into their window's surface and call `wm_invalidate()`. The window
manager repaints dirty windows, records damaged rectangles (merging those that
overlap), and composites wallpaper → windows (with shadows) → snap preview →
taskbar → Start menu → toasts → cursor for each damaged rectangle before
copying them to the framebuffer.

## Limitations

StreakOS is a real OS but a young one. It does not replace Windows.

* 32-bit, single core, everything runs in ring 0 with no memory protection
  between apps (cooperative: apps are callbacks in the compositor loop).
* Files live in RAM and are lost on shutdown. There are no disk, USB,
  network or sound drivers yet.
* Graphics are software-rendered into the firmware framebuffer, so the
  resolution is whatever GRUB sets (1280×800 requested).
* It can't run Windows programs.

## Licence and credits

StreakOS is part of this repository and is under the repository's licence.
The UI fonts are [Inter](https://rsms.me/inter/) and
[JetBrains Mono](https://www.jetbrains.com/lp/mono/), both under the SIL Open
Font License 1.1. They are pre-rendered into `kernel/fontdata.h`.
