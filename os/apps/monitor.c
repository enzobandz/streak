/* System Monitor: live frame-time graph, memory and system information. */
#include "../kernel/wm.h"

typedef struct { uint32_t last; char cpu[48]; } mon_t;

static void m_init(window_t *w) {
    mon_t *m = kzalloc(sizeof *m);
    w->data = m;
    cpu_name(m->cpu);
}

static void card(surface_t *s, rect_t r, const char *title) {
    theme_t *t = &g_theme;
    fill_round_rect(s, r, 10, t->dark ? RGB(0x28, 0x28, 0x2D) : RGB(0xF3, 0xF3, 0xF6));
    stroke_round_rect(s, r, 10, t->dark ? 0x12FFFFFF : 0x10000000);
    draw_text(s, r.x + 16, r.y + 12, title, &font_small, t->text_dim);
}

static void m_paint(window_t *w, surface_t *s) {
    mon_t *m = w->data;
    theme_t *t = &g_theme;
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->win_bg);
    int W = s->w - 32;
    char b[96];

    /* frame time graph */
    rect_t g = mkrect(16, 16, W, 190);
    card(s, g, "Frame rate");
    ksnprintf(b, sizeof b, "%u fps", g_fps);
    draw_text(s, g.x + 16, g.y + 32, b, &font_title, t->text);
    rect_t plot = mkrect(g.x + 16, g.y + 74, g.w - 32, g.h - 90);
    for (int i = 0; i <= 2; i++) fill_rect(s, mkrect(plot.x, plot.y + plot.h * i / 2, plot.w, 1), t->dark ? 0x14FFFFFF : 0x10000000);
    draw_text(s, plot.x + plot.w - 64, plot.y - 2, "33 ms", &font_small, t->text_dim);
    int prevx = -1, prevy = 0;
    for (int i = 0; i < 120; i++) {
        uint32_t v = g_frame_ms_hist[(g_frame_hist_pos + i) % 120];
        int x = plot.x + plot.w * i / 119;
        int y = plot.y + plot.h - (int)(MIN(v, 33u) * plot.h / 33);
        fill_rect(s, mkrect(x, y, plot.w / 119 + 1, plot.y + plot.h - y), WITH_ALPHA(t->accent, 0x30));
        if (prevx >= 0) draw_line(s, prevx, prevy, x, y, 2, t->accent);
        prevx = x; prevy = y;
    }

    /* memory */
    rect_t mr = mkrect(16, 222, W, 92);
    card(s, mr, "Memory (kernel heap)");
    size_t used = heap_used(), total = heap_total();
    ksnprintf(b, sizeof b, "%u.%u MiB of %u MiB", (unsigned)(used >> 20), (unsigned)((used >> 10) % 1024 * 10 / 1024), (unsigned)(total >> 20));
    draw_text(s, mr.x + 16, mr.y + 32, b, &font_bold, t->text);
    rect_t bar = mkrect(mr.x + 16, mr.y + 62, mr.w - 32, 10);
    fill_round_rect(s, bar, 5, t->dark ? 0x20FFFFFF : 0x14000000);
    int fw = (int)((unsigned long long)bar.w * used / (total ? total : 1));
    fill_round_rect(s, mkrect(bar.x, bar.y, MAX(fw, 10), bar.h), 5, t->accent);

    /* info */
    rect_t ir = mkrect(16, 330, W, s->h - 346);
    card(s, ir, "System");
    uint32_t sec = ticks_ms() / 1000;
    const char *keys[5] = {"Processor", "Display", "Uptime", "Windows", "Pointer"};
    char vals[5][64];
    strlcpy(vals[0], m->cpu, 64);
    ksnprintf(vals[1], 64, "%d x %d, 32-bit", SCR_W, SCR_H);
    ksnprintf(vals[2], 64, "%u:%02u:%02u", sec / 3600, (sec / 60) % 60, sec % 60);
    ksnprintf(vals[3], 64, "%d open", wm_window_count());
    strlcpy(vals[4], mouse_is_absolute() ? "Absolute (VM integration)" : "PS/2 mouse", 64);
    for (int i = 0; i < 5 && 36 + i * 24 < ir.h - 10; i++) {
        draw_text(s, ir.x + 16, ir.y + 36 + i * 24, keys[i], &font_ui, t->text_dim);
        draw_text(s, ir.x + 130, ir.y + 36 + i * 24, vals[i], &font_ui, t->text);
    }
}

static void m_tick(window_t *w, uint32_t now) {
    mon_t *m = w->data;
    if (now - m->last >= 100) { m->last = now; wm_invalidate(w); }
}

const app_t app_monitor = {"System Monitor", "Performance and system info", ICON_MONITOR, 520, 520, 1, 1,
                           m_init, m_paint, 0, 0, m_tick, 0};
