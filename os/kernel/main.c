/* StreakOS kernel entry: hardware bring-up, then the 60 fps compositor loop. */
#include "wm.h"
#include "fontdata.h"

extern char kernel_end[];
uint32_t g_frame_ms_hist[120];
int g_frame_hist_pos;
uint32_t g_fps;

const app_t *g_apps[] = {&app_terminal, &app_files, &app_notes, &app_paint, &app_calc, &app_monitor, &app_settings, &app_snake};
int g_app_count = sizeof g_apps / sizeof g_apps[0];

static void boot_splash(int progress) {
    clip_reset(g_screen);
    fill_rect(g_screen, mkrect(0, 0, SCR_W, SCR_H), RGB(0x07, 0x08, 0x10));
    int cx = SCR_W / 2, cy = SCR_H / 2 - 30;
    rect_t sq = mkrect(cx - 36, cy - 36, 72, 72);
    for (int i = 0; i < 72; i++) {
        rect_t saved = g_clip;
        g_clip = mkrect(sq.x, sq.y + i, 72, 1);
        fill_round_rect(g_screen, sq, 18, mix(RGB(0x7C, 0xC4, 0xFF), RGB(0x2B, 0x63, 0xF0), i * 255 / 71));
        g_clip = saved;
    }
    draw_line(g_screen, cx - 16, cy + 16, cx + 16, cy - 16, 9, 0xFFFFFFFF);
    fill_circle(g_screen, cx + 19, cy - 19, 5, 0xFFFFFFFF);
    draw_text_centered(g_screen, mkrect(0, cy + 56, SCR_W, 40), "StreakOS", &font_title, 0xFFFFFFFF);
    rect_t bar = mkrect(cx - 80, cy + 110, 160, 4);
    fill_round_rect(g_screen, bar, 2, 0x30FFFFFF);
    fill_round_rect(g_screen, mkrect(bar.x, bar.y, bar.w * progress / 100, 4), 2, RGB(0x7C, 0xC4, 0xFF));
    display_flush(mkrect(0, 0, SCR_W, SCR_H));
}

void kmain(uint32_t magic, uint32_t mbi) {
    serial_init();
    klog("StreakOS booting\n");
    if (magic != 0x36D76289) { klog("not loaded by a multiboot2 loader\n"); return; }

    /* parse multiboot2 tags */
    fb_info_t fb = {0};
    uintptr_t mem_lo = 0, mem_hi = 0;
    uint32_t total = *(uint32_t *)mbi;
    uintptr_t kend = ((uintptr_t)kernel_end + 0xFFF) & ~0xFFFu;
    uintptr_t mbi_end = mbi + total;
    for (uint8_t *p = (uint8_t *)mbi + 8; p < (uint8_t *)mbi + total;) {
        uint32_t type = ((uint32_t *)p)[0], size = ((uint32_t *)p)[1];
        if (type == 0) break;
        if (type == 8) { /* framebuffer */
            uint64_t addr = *(uint64_t *)(p + 8);
            fb.addr = (uintptr_t)addr;
            fb.pitch = *(uint32_t *)(p + 16);
            fb.width = *(uint32_t *)(p + 20);
            fb.height = *(uint32_t *)(p + 24);
            fb.bpp = *(uint8_t *)(p + 28);
        } else if (type == 6) { /* memory map: choose the largest usable region above 1 MiB */
            uint32_t esz = ((uint32_t *)p)[2];
            for (uint8_t *e = p + 16; e < p + size; e += esz) {
                uint64_t base = *(uint64_t *)e, len = *(uint64_t *)(e + 8);
                uint32_t etype = *(uint32_t *)(e + 16);
                if (etype != 1 || base >= 0xFFFFFFFFull) continue;
                uint64_t end = base + len;
                if (end > 0xFFFFFFFFull) end = 0xFFFFF000ull;
                uint64_t start = base;
                if (start < kend && end > kend) start = kend;
                if (start < mbi_end && end > mbi_end && mbi >= base && mbi < end) start = MAX(start, (uint64_t)mbi_end);
                if (start < 0x100000) continue;
                /* keep away from the framebuffer */
                if (end - start > mem_hi - mem_lo) { mem_lo = (uintptr_t)start; mem_hi = (uintptr_t)end; }
            }
        }
        p += (size + 7) & ~7u;
    }
    if (mem_lo < mbi_end && mem_hi > mbi_end) mem_lo = (mbi_end + 0xFFF) & ~0xFFFu;
    klog("fb %ux%u bpp=%u pitch=%u at %x; heap %x-%x\n", fb.width, fb.height, fb.bpp, fb.pitch, fb.addr, mem_lo, mem_hi);
    if (!fb.addr || mem_hi - mem_lo < (24u << 20)) { klog("need a framebuffer and >= 24 MiB RAM\n"); return; }

    heap_init(mem_lo, mem_hi);
    gdt_init();
    idt_init();
    display_init(&fb);
    boot_splash(10);
    timer_init(1000);
    keyboard_init();
    mouse_init();
    sti();
    fs_init();
    boot_splash(35);
    theme_set(1, 0);
    shell_init();
    boot_splash(100);
    wm_init();

    /* fade from the splash into the desktop */
    uint32_t start = ticks_ms(), last = 0, fps_t = start, frames = 0;
    toast("Welcome to StreakOS", "Press the Windows key or click Start.");
    wm_open(&app_terminal);
    klog("desktop ready after %u ms\n", ticks_ms());
    for (;;) {
        uint32_t now = ticks_ms();
        if (now - last >= 16) {
            uint32_t t0 = now;
            wm_frame(now);
            uint32_t t1 = ticks_ms();
            g_frame_ms_hist[g_frame_hist_pos] = MAX(t1 - t0, now - last > 100 ? 16 : now - last);
            g_frame_hist_pos = (g_frame_hist_pos + 1) % 120;
            last = now;
            frames++;
            if (now - fps_t >= 1000) { g_fps = frames * 1000 / (now - fps_t); frames = 0; fps_t = now; }
        }
        hlt();
    }
}
