/* StreakOS kernel entry: hardware bring-up, then the 60 fps compositor loop. */
#include "wm.h"
#include "fontdata.h"

extern char kernel_end[];
uint32_t g_frame_ms_hist[120];
int g_frame_hist_pos;
uint32_t g_fps;

const app_t *g_apps[] = {&app_terminal, &app_files, &app_notes, &app_paint, &app_calc, &app_monitor, &app_settings, &app_snake};
int g_app_count = sizeof g_apps / sizeof g_apps[0];

/* ---- boot-time memory selection: largest usable range, minus kernel and boot info ---- */
static uintptr_t kend_g, mbi_g, mbi_end_g;
static uint64_t best_lo, best_hi;
static void consider(uint64_t s, uint64_t e) {
    if (s < 0x100000) s = 0x100000;
    if (e > 0xFFFFF000ull) e = 0xFFFFF000ull;
    s = (s + 0xFFF) & ~0xFFFull;
    e &= ~0xFFFull;
    if (e <= s) return;
    uint64_t ex[2][2] = {{0x100000, kend_g}, {mbi_g & ~0xFFFull, (mbi_end_g + 0xFFF) & ~0xFFFull}};
    for (int i = 0; i < 2; i++)
        if (s < ex[i][1] && e > ex[i][0]) { consider(s, ex[i][0]); consider(ex[i][1], e); return; }
    if (e - s > best_hi - best_lo) { best_lo = s; best_hi = e; }
}

/* Errors before the desktop exists: VGA text mode (BIOS) or the raw framebuffer. */
static void text_error(const char *msg) {
    klog("%s\n", msg);
    volatile uint16_t *vga = (uint16_t *)0xB8000;
    for (int i = 0; i < 80 * 25; i++) vga[i] = 0x1F00 | ' ';
    for (int i = 0; msg[i] && i < 80 * 25; i++) vga[80 * 2 + 2 + i] = 0x1F00 | (uint8_t)msg[i];
}
static void fb_error(fb_info_t *fb, const char *msg) {
    if (fb->bpp != 32) return;
    surface_t s = {(uint32_t *)fb->addr, (int)fb->width, (int)fb->height, (int)(fb->pitch / 4)};
    clip_reset(&s);
    fill_rect(&s, mkrect(0, 0, s.w, s.h), RGB(0x12, 0x1A, 0x3A));
    draw_text(&s, 60, 80, "StreakOS can't start", &font_title, 0xFFFFFFFF);
    draw_text(&s, 60, 130, msg, &font_ui, 0xFFDDDDFF);
}

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
    uint64_t fb_addr = 0;
    uint32_t total = *(uint32_t *)mbi;
    kend_g = ((uintptr_t)kernel_end + 0xFFF) & ~0xFFFu;
    mbi_g = mbi;
    mbi_end_g = mbi + total;
    for (uint8_t *p = (uint8_t *)mbi + 8; p < (uint8_t *)mbi + total;) {
        uint32_t type = ((uint32_t *)p)[0], size = ((uint32_t *)p)[1];
        if (type == 0) break;
        if (type == 8) { /* framebuffer */
            fb_addr = *(uint64_t *)(p + 8);
            fb.pitch = *(uint32_t *)(p + 16);
            fb.width = *(uint32_t *)(p + 20);
            fb.height = *(uint32_t *)(p + 24);
            fb.bpp = *(uint8_t *)(p + 28);
            if (p[29] != 1) fb.bpp = 0; /* type 1 = direct RGB; indexed / EGA text are unsupported */
            else fb.bgr = p[32] == 0 && p[36] >= 16; /* red at bit 0: BGR byte order */
        } else if (type == 6) { /* memory map */
            uint32_t esz = ((uint32_t *)p)[2];
            for (uint8_t *e = p + 16; e + 24 <= p + size; e += esz)
                if (*(uint32_t *)(e + 16) == 1) consider(*(uint64_t *)e, *(uint64_t *)e + *(uint64_t *)(e + 8));
        } else if (type == 14 || type == 15) { /* ACPI RSDP copy */
            acpi_set_rsdp((uintptr_t)(p + 8));
        }
        p += (size + 7) & ~7u;
    }
    klog("fb %ux%u bpp=%u pitch=%u at %x; heap %x-%x\n", fb.width, fb.height, fb.bpp, fb.pitch, (uint32_t)fb_addr, (uint32_t)best_lo, (uint32_t)best_hi);
    if (!fb_addr || !fb.width) { text_error("StreakOS needs a graphics mode. Boot it with GRUB from the StreakOS ISO."); return; }
    if (fb_addr + (uint64_t)fb.pitch * fb.height > 0xFFFFFFFFull) { klog("framebuffer above 4 GiB is not supported\n"); return; }
    fb.addr = (uintptr_t)fb_addr;
    if (fb.bpp != 32 && fb.bpp != 24 && fb.bpp != 16) {
        klog("unsupported framebuffer format (%u bpp)\n", fb.bpp);
        return;
    }
    uint64_t need = (uint64_t)fb.width * fb.height * 4 * 3 + (20u << 20);
    if (best_hi - best_lo < need) {
        char msg[96];
        ksnprintf(msg, sizeof msg, "StreakOS needs at least %u MiB of RAM at this resolution.", (unsigned)(need >> 20) + 4);
        klog("%s\n", msg);
        fb_error(&fb, msg);
        return;
    }
    uintptr_t mem_lo = (uintptr_t)best_lo, mem_hi = (uintptr_t)best_hi;
    heap_init(mem_lo, mem_hi);
    gdt_init();
    acpi_init();
    idt_init();
    display_init(&fb);
    boot_splash(10);
    timer_init(1000);
    keyboard_init();
    mouse_init();
    sti();
    fs_init();
    boot_splash(35);
    shell_init(); /* applies saved theme and wallpaper */
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
