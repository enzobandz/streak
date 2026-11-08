/* Settings: theme, accent colour, wallpaper and animation speed. */
#include "../kernel/wm.h"

const char *wallpaper_name(int i);
typedef struct { anim_t knob; int hover; } set_t;
static const int speeds[3] = {0, 100, 50};
static const char *speed_names[3] = {"Off", "Normal", "Relaxed"};

static rect_t toggle_r(void) { return mkrect(0, 0, 44, 22); }
#define SEC_DARK 40
#define SEC_ACC 110
#define SEC_WALL 206
#define SEC_ANIM 320

static rect_t dark_toggle(int w) { rect_t r = toggle_r(); r.x = w - 16 - 24 - r.w; r.y = SEC_DARK + 9; return r; }
static rect_t acc_r(int i) { return mkrect(40 + i * 44, SEC_ACC + 36, 32, 32); }
static rect_t wall_r(int i, int w) { int tw = (w - 64 - 3 * 12) / 4; return mkrect(32 + i * (tw + 12), SEC_WALL + 34, tw, tw * 10 / 16); }
static rect_t speed_r(int i) { return mkrect(32 + i * 104, SEC_ANIM + 34, 100, 34); }

static void s_init(window_t *w) {
    set_t *s = kzalloc(sizeof *s);
    w->data = s;
    s->hover = -1;
    s->knob.to = g_theme.dark;
}

static void section(surface_t *s, int y, int h, const char *title) {
    theme_t *t = &g_theme;
    rect_t r = mkrect(16, y, s->w - 32, h);
    fill_round_rect(s, r, 10, t->dark ? RGB(0x28, 0x28, 0x2D) : RGB(0xF3, 0xF3, 0xF6));
    draw_text(s, 32, y + 12, title, &font_bold, t->text);
}

static void s_paint(window_t *w, surface_t *s) {
    set_t *st = w->data;
    theme_t *t = &g_theme;
    uint32_t now = ticks_ms();
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->win_bg);
    draw_text(s, 18, 8, "Personalisation", &font_title, t->text);

    section(s, SEC_DARK, 56, "Dark mode");
    {
        rect_t tg = dark_toggle(s->w);
        tg.y = SEC_DARK + 17;
        float k = anim_value(&st->knob, now);
        uint32_t bg = mix(t->dark ? RGB(0x50, 0x50, 0x58) : RGB(0xC8, 0xC8, 0xD0), t->accent, (int)(k * 255));
        fill_round_rect(s, tg, 11, bg);
        int kx = tg.x + 4 + (int)(k * (tg.w - 22));
        fill_circle(s, kx + 7, tg.y + 11, st->hover == 1 ? 8 : 7, 0xFFFFFFFF);
    }

    section(s, SEC_ACC, 88, "Accent colour");
    for (int i = 0; i < 6; i++) {
        rect_t r = acc_r(i);
        if (i == g_accent_idx) stroke_round_rect(s, mkrect(r.x - 4, r.y - 4, r.w + 8, r.h + 8), 20, t->text);
        fill_circle(s, r.x + 16, r.y + 16, st->hover == 10 + i ? 15 : 14, g_accents[i]);
    }

    int tw = wall_r(0, s->w).h;
    section(s, SEC_WALL, 34 + tw + 50, "Wallpaper");
    static const uint32_t wcols[4][3] = {
        {RGB(0x09, 0x0E, 0x2A), RGB(0x00, 0xC9, 0xA7), RGB(0xB4, 0x4D, 0xFF)},
        {RGB(0x2B, 0x0F, 0x2E), RGB(0xFF, 0x7A, 0x45), RGB(0xFF, 0x3D, 0x7F)},
        {RGB(0x03, 0x1B, 0x2E), RGB(0x00, 0xB4, 0xD8), RGB(0x00, 0x5F, 0xB6)},
        {RGB(0xE6, 0xEE, 0xFF), RGB(0x9C, 0xC2, 0xFF), RGB(0xFF, 0xBF, 0xDF)},
    };
    for (int i = 0; i < 4; i++) {
        rect_t r = wall_r(i, s->w);
        surface_t th = surface_view(s, r);
        fill_rect(&th, mkrect(0, 0, r.w, r.h), wcols[i][0]);
        fill_circle(&th, r.w / 4, r.h / 3, r.h / 2, WITH_ALPHA(wcols[i][1], 0xB0));
        fill_circle(&th, r.w * 3 / 4, r.h * 3 / 4, r.h / 2, WITH_ALPHA(wcols[i][2], 0xB0));
        /* round the thumbnail corners by painting the section colour back outside the arc */
        uint32_t secbg = t->dark ? RGB(0x28, 0x28, 0x2D) : RGB(0xF3, 0xF3, 0xF6);
        for (int cy = 0; cy < 8; cy++)
            for (int cx = 0; cx < 8; cx++) {
                int dx = 8 - cx, dy = 8 - cy;
                if (dx * dx + dy * dy <= 64) continue;
                th.px[cy * th.stride + cx] = secbg;
                th.px[cy * th.stride + r.w - 1 - cx] = secbg;
                th.px[(r.h - 1 - cy) * th.stride + cx] = secbg;
                th.px[(r.h - 1 - cy) * th.stride + r.w - 1 - cx] = secbg;
            }
        stroke_round_rect(s, r, 8, i == g_wallpaper_idx ? t->accent : (st->hover == 20 + i ? t->text_dim : t->border));
        if (i == g_wallpaper_idx) stroke_round_rect(s, mkrect(r.x + 1, r.y + 1, r.w - 2, r.h - 2), 7, t->accent);
        draw_text_centered(s, mkrect(r.x, r.y + r.h + 6, r.w, 20), wallpaper_name(i), &font_small, t->text_dim);
    }
    int y2 = SEC_WALL + 34 + tw + 50 + 16;
    int anim_y = y2;
    section(s, anim_y, 84, "Animations");
    for (int i = 0; i < 3; i++) {
        rect_t r = speed_r(i);
        r.y = anim_y + 36;
        ui_button(s, r, speed_names[i], st->hover == 30 + i, 0, g_anim_speed_pct == speeds[i]);
    }
    draw_text(s, 32, anim_y + 84 + 18, "StreakOS 1.0  -  everything here is drawn by the StreakOS compositor.", &font_small, t->text_dim);
}

static int hit(window_t *w, int x, int y) {
    int cw = w->w - 2;
    rect_t tg = dark_toggle(cw);
    tg.y = SEC_DARK + 17;
    if (rect_contains(mkrect(16, SEC_DARK, cw - 32, 56), x, y)) return 1;
    (void)tg;
    for (int i = 0; i < 6; i++) if (rect_contains(acc_r(i), x, y)) return 10 + i;
    for (int i = 0; i < 4; i++) if (rect_contains(wall_r(i, cw), x, y)) return 20 + i;
    int anim_y = SEC_WALL + 34 + wall_r(0, cw).h + 50 + 16;
    for (int i = 0; i < 3; i++) { rect_t r = speed_r(i); r.y = anim_y + 36; if (rect_contains(r, x, y)) return 30 + i; }
    return -1;
}

static void s_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    (void)b; (void)wheel;
    set_t *st = w->data;
    int h = hit(w, x, y);
    if (h != st->hover) { st->hover = h; wm_invalidate(w); }
    if (ev != ME_DOWN || h < 0) return;
    if (h == 1) {
        int dark = !g_theme.dark;
        anim_start(&st->knob, anim_value(&st->knob, ticks_ms()), dark, 260, EASE_OUT_BACK);
        theme_set(dark, g_accent_idx);
        if (!dark && g_wallpaper_idx != 3) shell_generate_wallpaper(3);
        else if (dark && g_wallpaper_idx == 3) shell_generate_wallpaper(0);
        wm_apply_theme();
    } else if (h >= 10 && h < 16) {
        theme_set(g_theme.dark, h - 10);
        wm_apply_theme();
    } else if (h >= 20 && h < 24) {
        shell_generate_wallpaper(h - 20);
        wm_apply_theme();
    } else if (h >= 30 && h < 33) {
        g_anim_speed_pct = speeds[h - 30];
    }
    wm_invalidate(w);
}

static void s_tick(window_t *w, uint32_t now) {
    (void)now;
    set_t *st = w->data;
    if (st->knob.active) wm_invalidate(w);
}

const app_t app_settings = {"Settings", "Theme, colours and wallpaper", ICON_SETTINGS, 560, 560, 1, 1,
                            s_init, s_paint, 0, s_mouse, s_tick, 0};
