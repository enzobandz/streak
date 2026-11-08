/* Desktop shell: theme, generated wallpaper, app icons, taskbar with animated
 * indicators, acrylic Start menu with search, and toast notifications. */
#include "wm.h"

theme_t g_theme;
int g_accent_idx, g_wallpaper_idx;
const uint32_t g_accents[6] = {RGB(0x3D, 0x9B, 0xFF), RGB(0x8B, 0x5C, 0xF6), RGB(0xEC, 0x48, 0x99),
                               RGB(0xF9, 0x73, 0x16), RGB(0x10, 0xB9, 0x81), RGB(0xEA, 0xB3, 0x08)};

static surface_t *wall, *wall_blur;

void theme_set(int dark, int accent) {
    theme_t *t = &g_theme;
    g_accent_idx = accent;
    t->dark = dark;
    t->accent = g_accents[accent];
    t->accent_text = 0xFFFFFFFF;
    if (dark) {
        t->win_bg = RGB(0x20, 0x20, 0x22); t->title_bg = RGB(0x1B, 0x1B, 0x1D);
        t->text = RGB(0xF5, 0xF5, 0xF7); t->text_dim = RGB(0x98, 0x98, 0xA0);
        t->border = 0x30FFFFFF; t->hover = 0x18FFFFFF; t->press = 0x0EFFFFFF;
        t->field = RGB(0x2B, 0x2B, 0x2F);
        t->taskbar_tint = 0xB0141418; t->menu_tint = 0xC01C1C20;
    } else {
        t->win_bg = RGB(0xFA, 0xFA, 0xFB); t->title_bg = RGB(0xF1, 0xF1, 0xF4);
        t->text = RGB(0x1A, 0x1A, 0x1E); t->text_dim = RGB(0x6A, 0x6A, 0x74);
        t->border = 0x28000000; t->hover = 0x12000000; t->press = 0x20000000;
        t->field = RGB(0xFF, 0xFF, 0xFF);
        t->taskbar_tint = 0xB8F2F2F6; t->menu_tint = 0xCCF4F4F8;
    }
}

/* ---- wallpaper ---- */
typedef struct { float x, y, r; uint32_t c; } blob_t;
typedef struct { const char *name; uint32_t top, bottom; blob_t b[4]; float streak; } wallspec_t;
static const wallspec_t walls[4] = {
    {"Aurora", RGB(0x09, 0x0E, 0x2A), RGB(0x17, 0x0A, 0x30),
     {{0.18f, 0.30f, 0.50f, RGB(0x00, 0xC9, 0xA7)}, {0.78f, 0.22f, 0.55f, RGB(0x4F, 0x6B, 0xFF)},
      {0.62f, 0.85f, 0.50f, RGB(0xB4, 0x4D, 0xFF)}, {0.10f, 0.92f, 0.38f, RGB(0xFF, 0x5E, 0x9A)}}, 0.16f},
    {"Sunset", RGB(0x2B, 0x0F, 0x2E), RGB(0x10, 0x08, 0x1E),
     {{0.25f, 0.75f, 0.55f, RGB(0xFF, 0x7A, 0x45)}, {0.80f, 0.70f, 0.50f, RGB(0xFF, 0x3D, 0x7F)},
      {0.50f, 0.95f, 0.40f, RGB(0xFF, 0xC1, 0x5E)}, {0.70f, 0.15f, 0.45f, RGB(0x7A, 0x3C, 0xFF)}}, 0.12f},
    {"Ocean", RGB(0x03, 0x1B, 0x2E), RGB(0x01, 0x08, 0x14),
     {{0.20f, 0.20f, 0.50f, RGB(0x00, 0xB4, 0xD8)}, {0.85f, 0.55f, 0.55f, RGB(0x00, 0x5F, 0xB6)},
      {0.45f, 0.90f, 0.45f, RGB(0x48, 0xCA, 0xE4)}, {0.95f, 0.05f, 0.30f, RGB(0x6E, 0x5C, 0xFF)}}, 0.10f},
    {"Bloom", RGB(0xE6, 0xEE, 0xFF), RGB(0xF7, 0xEC, 0xFA),
     {{0.20f, 0.25f, 0.55f, RGB(0x9C, 0xC2, 0xFF)}, {0.80f, 0.30f, 0.50f, RGB(0xFF, 0xBF, 0xDF)},
      {0.60f, 0.85f, 0.55f, RGB(0xC4, 0xAE, 0xFF)}, {0.10f, 0.90f, 0.40f, RGB(0xA8, 0xEE, 0xDE)}}, 0.25f},
};
const char *wallpaper_name(int i) { return walls[i & 3].name; }

static void upscale(surface_t *lo, surface_t *hi) {
    uint32_t sx = ((uint32_t)(lo->w - 1) << 16) / (hi->w - 1);
    uint32_t sy = ((uint32_t)(lo->h - 1) << 16) / (hi->h - 1);
    uint32_t seed = 12345;
    for (int y = 0; y < hi->h; y++) {
        uint32_t fy = y * sy;
        int y0 = fy >> 16, y1 = MIN(y0 + 1, lo->h - 1);
        uint32_t wy = (fy >> 8) & 255;
        for (int x = 0; x < hi->w; x++) {
            uint32_t fx = x * sx;
            int x0 = fx >> 16, x1 = MIN(x0 + 1, lo->w - 1);
            uint32_t wx = (fx >> 8) & 255;
            uint32_t a = lo->px[y0 * lo->w + x0], b = lo->px[y0 * lo->w + x1];
            uint32_t c = lo->px[y1 * lo->w + x0], d = lo->px[y1 * lo->w + x1];
            seed = seed * 1103515245 + 12345;
            int n = (int)((seed >> 16) & 3) - 1; /* dither against banding */
            uint32_t out = 0xFF000000;
            for (int sh = 0; sh <= 16; sh += 8) {
                int ca = (a >> sh) & 255, cb = (b >> sh) & 255, cc = (c >> sh) & 255, cd = (d >> sh) & 255;
                int top = ca * 256 + (cb - ca) * (int)wx, bot = cc * 256 + (cd - cc) * (int)wx;
                int v = (top * 256 + (bot - top) * (int)wy) >> 16;
                v = CLAMP(v + n, 0, 255);
                out |= (uint32_t)v << sh;
            }
            hi->px[y * hi->stride + x] = out;
        }
    }
}

void shell_generate_wallpaper(int idx) {
    const wallspec_t *ws = &walls[idx & 3];
    g_wallpaper_idx = idx & 3;
    int lw = SCR_W / 4, lh = SCR_H / 4;
    surface_t *lo = surface_new(lw, lh);
    float aspect = (float)SCR_H / SCR_W;
    for (int y = 0; y < lh; y++) {
        float fy = (float)y / lh;
        uint32_t base = mix(ws->top, ws->bottom, y * 255 / lh);
        float br = (base >> 16) & 255, bg = (base >> 8) & 255, bb = base & 255;
        for (int x = 0; x < lw; x++) {
            float fx = (float)x / lw;
            float r = br, g = bg, b = bb;
            for (int i = 0; i < 4; i++) {
                const blob_t *bl = &ws->b[i];
                float dx = fx - bl->x, dy = (fy - bl->y) * aspect;
                float d2 = (dx * dx + dy * dy) / (bl->r * bl->r);
                if (d2 >= 1) continue;
                float wgt = (1 - d2) * (1 - d2) * 0.85f;
                r += (((bl->c >> 16) & 255) - r) * wgt;
                g += (((bl->c >> 8) & 255) - g) * wgt;
                b += ((bl->c & 255) - b) * wgt;
            }
            /* the "streak": a soft diagonal band of light */
            float d = fy - (0.95f - 0.75f * fx);
            float band = 1 - (d * d) / (0.012f);
            if (band > 0) {
                float k = band * band * ws->streak;
                r += (255 - r) * k; g += (255 - g) * k; b += (255 - b) * k;
            }
            lo->px[y * lw + x] = RGB((int)r, (int)g, (int)b);
        }
    }
    box_blur(lo, 2);
    if (!wall) { wall = surface_new(SCR_W, SCR_H); wall_blur = surface_new(SCR_W, SCR_H); }
    upscale(lo, wall);
    box_blur(lo, 10);
    upscale(lo, wall_blur);
    surface_free(lo);
    wm_damage_all();
}

void shell_compose_wallpaper(rect_t r) { blit_opaque(g_screen, r.x, r.y, wall, r); }

/* "acrylic": blurred wallpaper behind + tint */
static void acrylic(surface_t *dst, rect_t screen_r, int dx, int dy, uint32_t tint) {
    blit_opaque(dst, dx, dy, wall_blur, screen_r);
    fill_rect(dst, mkrect(dx, dy, screen_r.w, screen_r.h), tint);
}

/* ---- icons ---- */
void draw_app_icon(surface_t *s, int x, int y, int sz, int icon) {
    static const uint32_t grad[ICON_COUNT][2] = {
        {RGB(0x3A, 0x3A, 0x44), RGB(0x14, 0x14, 0x1A)}, {RGB(0xFF, 0xD8, 0x5C), RGB(0xFF, 0xA9, 0x1F)},
        {RGB(0xFF, 0x6F, 0xB5), RGB(0x8E, 0x4D, 0xFF)}, {RGB(0x55, 0xB5, 0xFF), RGB(0x2B, 0x63, 0xF0)},
        {RGB(0x3C, 0xE0, 0xA8), RGB(0x0E, 0x9F, 0x8A)}, {RGB(0x9C, 0xA3, 0xB4), RGB(0x5B, 0x63, 0x77)},
        {RGB(0x8B, 0xE8, 0x5E), RGB(0x2E, 0xA0, 0x43)}, {RGB(0x6C, 0xC4, 0xFF), RGB(0x2F, 0x86, 0xE8)},
    };
    int rad = sz * 22 / 100;
    rect_t r = mkrect(x, y, sz, sz);
    /* gradient rounded square: fill rows, masked by a rounded rect */
    for (int i = 0; i < sz; i++) {
        uint32_t c = mix(grad[icon][0], grad[icon][1], i * 255 / sz);
        rect_t row = mkrect(x, y + i, sz, 1);
        rect_t saved = g_clip;
        g_clip = rect_intersect(g_clip, row);
        fill_round_rect(s, r, rad, c);
        g_clip = saved;
    }
    stroke_round_rect(s, r, rad, 0x30FFFFFF);
    int u = sz; /* unit */
    uint32_t W = 0xF0FFFFFF;
    switch (icon) {
    case ICON_TERMINAL:
        draw_line(s, x + u * 25 / 100, y + u * 33 / 100, x + u * 42 / 100, y + u * 48 / 100, MAX(2, u / 14), RGB(0x6E, 0xF0, 0xA0));
        draw_line(s, x + u * 42 / 100, y + u * 48 / 100, x + u * 25 / 100, y + u * 63 / 100, MAX(2, u / 14), RGB(0x6E, 0xF0, 0xA0));
        fill_round_rect(s, mkrect(x + u * 48 / 100, y + u * 60 / 100, u * 26 / 100, MAX(2, u / 12)), 1, W);
        break;
    case ICON_NOTES:
        for (int i = 0; i < 3; i++)
            fill_round_rect(s, mkrect(x + u * 24 / 100, y + u * (30 + i * 16) / 100, u * (i == 2 ? 34 : 52) / 100, MAX(2, u / 12)), 1, RGB(0x7A, 0x4A, 0x00));
        break;
    case ICON_PAINT:
        fill_circle(s, x + u * 50 / 100, y + u * 50 / 100, u * 30 / 100, W);
        fill_circle(s, x + u * 38 / 100, y + u * 40 / 100, u * 7 / 100, RGB(0xFF, 0x4D, 0x6D));
        fill_circle(s, x + u * 57 / 100, y + u * 36 / 100, u * 7 / 100, RGB(0x3D, 0x9B, 0xFF));
        fill_circle(s, x + u * 64 / 100, y + u * 55 / 100, u * 7 / 100, RGB(0x2E, 0xC4, 0x6B));
        fill_circle(s, x + u * 42 / 100, y + u * 62 / 100, u * 6 / 100, RGB(0xFF, 0xB0, 0x20));
        break;
    case ICON_CALC: {
        int c = u * 15 / 100, g = u * 6 / 100, ox = x + (u - 3 * c - 2 * g) / 2, oy = y + (u - 3 * c - 2 * g) / 2;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
                fill_round_rect(s, mkrect(ox + i * (c + g), oy + j * (c + g), c, c), MAX(1, c / 4), (i == 2 && j == 2) ? RGB(0xFF, 0xB0, 0x20) : W);
        break;
    }
    case ICON_MONITOR: {
        int bw = u * 11 / 100, g = u * 6 / 100, ox = x + (u - 4 * bw - 3 * g) / 2, base = y + u * 74 / 100;
        int hs[4] = {30, 48, 22, 40};
        for (int i = 0; i < 4; i++) fill_round_rect(s, mkrect(ox + i * (bw + g), base - u * hs[i] / 100, bw, u * hs[i] / 100), MAX(1, bw / 3), W);
        break;
    }
    case ICON_SETTINGS: {
        int cx = x + u / 2, cy = y + u / 2, R = u * 26 / 100;
        static const int dx8[8] = {100, 71, 0, -71, -100, -71, 0, 71}, dy8[8] = {0, 71, 100, 71, 0, -71, -100, -71};
        for (int i = 0; i < 8; i++) fill_circle(s, cx + R * dx8[i] / 100, cy + R * dy8[i] / 100, MAX(2, u * 8 / 100), W);
        fill_circle(s, cx, cy, R, W);
        fill_circle(s, cx, cy, u * 11 / 100, mix(grad[icon][0], grad[icon][1], 128));
        break;
    }
    case ICON_SNAKE: {
        int r = MAX(2, u * 8 / 100);
        int px[5] = {26, 40, 54, 62, 70}, py[5] = {62, 62, 58, 46, 34};
        for (int i = 0; i < 5; i++) fill_circle(s, x + u * px[i] / 100, y + u * py[i] / 100, r, W);
        fill_circle(s, x + u * 30 / 100, y + u * 32 / 100, r, RGB(0xFF, 0x4D, 0x6D));
        break;
    }
    case ICON_FILES:
        fill_round_rect(s, mkrect(x + u * 20 / 100, y + u * 28 / 100, u * 28 / 100, u * 16 / 100), MAX(1, u / 16), RGB(0xFF, 0xE0, 0x8A));
        fill_round_rect(s, mkrect(x + u * 20 / 100, y + u * 36 / 100, u * 60 / 100, u * 40 / 100), MAX(1, u / 14), RGB(0xFF, 0xD0, 0x5C));
        break;
    }
}

/* ---- shared widget ---- */
void ui_button(surface_t *s, rect_t r, const char *label, int hover, int pressed, int primary) {
    theme_t *t = &g_theme;
    uint32_t bg;
    if (primary) bg = pressed ? mix(t->accent, 0xFF000000, 40) : hover ? mix(t->accent, 0xFFFFFFFF, 30) : t->accent;
    else {
        bg = t->field;
        if (hover) bg = blend(bg, t->hover, 255);
        if (pressed) bg = blend(bg, t->press, 255);
    }
    fill_round_rect(s, r, 6, bg);
    if (!primary) stroke_round_rect(s, r, 6, t->border);
    draw_text_centered(s, r, label, &font_ui, primary ? t->accent_text : t->text);
}

/* ---- taskbar ---- */
#define SLOT 48
static int nslots;
static anim_t slot_hover[16], slot_ind[16];
static float slot_ind_target[16];
static int slot_hovered = -1, slot_pressed = -1;
static char clock_txt[16], date_txt[24];
static int last_min = -1;
static uint32_t last_rtc;

static rect_t taskbar_rect(void) { return mkrect(0, SCR_H - TASKBAR_H, SCR_W, TASKBAR_H); }
static rect_t slot_rect(int i) { return mkrect((SCR_W - nslots * SLOT) / 2 + i * SLOT + 2, SCR_H - TASKBAR_H + 4, SLOT - 4, SLOT - 4); }
static const app_t *slot_app(int i) { return i == 0 ? 0 : g_apps[i - 1]; }

rect_t shell_icon_rect_for(const app_t *app) {
    for (int i = 1; i < nslots; i++)
        if (slot_app(i) == app) { rect_t r = slot_rect(i); return mkrect(r.x + 6, r.y + 6, r.w - 12, r.h - 12); }
    return mkrect(SCR_W / 2 - 16, SCR_H - 40, 32, 32);
}

static window_t *top_window_of(const app_t *app, int *count) {
    window_t *best = 0;
    int n = 0;
    for (int i = 0; i < wm_window_count(); i++) {
        window_t *w = wm_window_at(i);
        if (w->app == app && w->state != WS_CLOSING) { best = w; n++; }
    }
    if (count) *count = n;
    return best;
}

void shell_windows_changed(void) {
    for (int i = 1; i < nslots; i++) {
        int n;
        window_t *w = top_window_of(slot_app(i), &n);
        float target = !w ? 0 : (w == wm_focused() ? 18 : 6);
        if (target != slot_ind_target[i]) {
            float cur = anim_value(&slot_ind[i], ticks_ms());
            anim_start(&slot_ind[i], cur, target, 260, EASE_OUT_BACK);
            slot_ind_target[i] = target;
        }
    }
    wm_damage(taskbar_rect());
}

static void draw_start_glyph(surface_t *s, rect_t r) {
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    rect_t sq = mkrect(cx - 13, cy - 13, 26, 26);
    for (int i = 0; i < 26; i++) {
        rect_t saved = g_clip;
        g_clip = rect_intersect(g_clip, mkrect(sq.x, sq.y + i, 26, 1));
        fill_round_rect(s, sq, 7, mix(mix(g_theme.accent, 0xFFFFFFFF, 60), mix(g_theme.accent, 0xFF000000, 50), i * 255 / 25));
        g_clip = saved;
    }
    draw_line(s, cx - 6, cy + 6, cx + 6, cy - 6, 4, 0xFFFFFFFF);
    fill_circle(s, cx + 7, cy - 7, 2, 0xFFFFFFFF);
}

static void draw_taskbar(uint32_t now) {
    rect_t tb = taskbar_rect();
    if (rect_empty(rect_intersect(tb, g_clip))) return;
    acrylic(g_screen, tb, tb.x, tb.y, g_theme.taskbar_tint);
    fill_rect(g_screen, mkrect(0, tb.y, SCR_W, 1), g_theme.dark ? 0x22FFFFFF : 0x18000000);
    for (int i = 0; i < nslots; i++) {
        rect_t r = slot_rect(i);
        float h = anim_value(&slot_hover[i], now);
        if (h > 0.01f) fill_round_rect(g_screen, r, 8, WITH_ALPHA(g_theme.dark ? 0xFFFFFF : 0x000000, (int)(h * (g_theme.dark ? 26 : 20))));
        int press = slot_pressed == i && slot_hovered == i;
        int sz = press ? 26 : 28;
        if (i == 0) draw_start_glyph(g_screen, r);
        else draw_app_icon(g_screen, r.x + (r.w - sz) / 2, r.y + (r.h - sz) / 2 - 2, sz, slot_app(i)->icon);
        float iw = anim_value(&slot_ind[i], now);
        if (iw > 0.5f) {
            int w = (int)iw;
            fill_round_rect(g_screen, mkrect(r.x + (r.w - w) / 2, r.y + r.h - 3, w, 3), 2,
                            iw > 10 ? g_theme.accent : (g_theme.dark ? 0xFF9A9AA4 : 0xFF70707A));
        }
    }
    /* clock */
    int tw = MAX(text_width(clock_txt, &font_small), text_width(date_txt, &font_small));
    int x = SCR_W - tw - 18;
    draw_text(g_screen, x + tw - text_width(clock_txt, &font_small), tb.y + 9, clock_txt, &font_small, g_theme.text);
    draw_text(g_screen, x + tw - text_width(date_txt, &font_small), tb.y + 27, date_txt, &font_small, g_theme.text);
    /* left: tiny brand */
    draw_text(g_screen, 18, tb.y + (TASKBAR_H - font_bold.height) / 2, "StreakOS", &font_bold, g_theme.text_dim);
}

/* ---- start menu ---- */
static surface_t *menu;
static int menu_open, menu_dirty = 1, menu_hover = -1, menu_sel, menu_power, menu_pressed = -1;
static anim_t menu_anim;
static char query[48];
static int qlen;
static rect_t menu_last;
#define MENU_W 620
static int menu_h(void) { return MIN(600, SCR_H - TASKBAR_H - 24); }
static rect_t menu_rect_base(void) { return mkrect((SCR_W - MENU_W) / 2, SCR_H - TASKBAR_H - 12 - menu_h(), MENU_W, menu_h()); }

/* hit regions: 0..31 apps (visible index), 100+ files, 200 power, 201 restart, 202 shutdown, 203 search */
static const app_t *visible[16];
static int nvisible;
static void filter_apps(void) {
    nvisible = 0;
    for (int i = 0; i < g_app_count; i++)
        if (!qlen || str_contains_ci(g_apps[i]->name, query) || str_contains_ci(g_apps[i]->desc, query)) visible[nvisible++] = g_apps[i];
    if (menu_sel >= nvisible) menu_sel = MAX(0, nvisible - 1);
}
static int recommended[4], nrecommended;
static rect_t app_cell(int i) { return mkrect(28 + (i % 6) * 94, 122 + (i / 6) * 92, 90, 88); }
static rect_t file_cell(int i) { return mkrect(28 + (i % 2) * 282, 352 + (i / 2) * 56, 278, 52); }
static rect_t power_btn(void) { return mkrect(MENU_W - 64, menu_h() - 54, 44, 44); }
static rect_t power_item(int i) { return mkrect(MENU_W - 200, menu_h() - 150 + i * 44, 180, 40); }

static void render_menu(void) {
    int H = menu_h();
    if (!menu || menu->h != H) { surface_free(menu); menu = surface_new(MENU_W, H); }
    theme_t *t = &g_theme;
    rect_t base = menu_rect_base();
    clip_reset(menu);
    acrylic(menu, base, 0, 0, t->menu_tint);
    /* search */
    rect_t sb = mkrect(24, 24, MENU_W - 48, 42);
    fill_round_rect(menu, sb, 21, t->dark ? 0xFF2A2A30 : 0xFFFFFFFF);
    stroke_round_rect(menu, sb, 21, qlen ? WITH_ALPHA(t->accent, 0xC0) : t->border);
    fill_circle(menu, sb.x + 24, sb.y + 19, 7, t->text_dim);
    fill_circle(menu, sb.x + 24, sb.y + 19, 5, t->dark ? 0xFF2A2A30 : 0xFFFFFFFF);
    draw_line(menu, sb.x + 29, sb.y + 24, sb.x + 33, sb.y + 28, 2, t->text_dim);
    int tx = sb.x + 46, ty = sb.y + (sb.h - font_ui.height) / 2;
    if (qlen) {
        int e = draw_text(menu, tx, ty, query, &font_ui, t->text);
        if ((ticks_ms() / 530) % 2 == 0) fill_rect(menu, mkrect(e + 1, ty + 1, 2, font_ui.height - 2), t->accent);
    } else {
        draw_text(menu, tx, ty, "Search apps and files", &font_ui, t->text_dim);
    }
    /* apps */
    draw_text(menu, 36, 88, qlen ? "Results" : "Pinned", &font_bold, t->text);
    if (!nvisible) draw_text(menu, 36, 130, "No apps match your search.", &font_ui, t->text_dim);
    for (int i = 0; i < nvisible; i++) {
        rect_t c = app_cell(i);
        if (menu_hover == i || (qlen && menu_sel == i)) fill_round_rect(menu, c, 10, menu_pressed == i ? t->press : t->hover);
        draw_app_icon(menu, c.x + (c.w - 44) / 2, c.y + 10, 44, visible[i]->icon);
        draw_text_centered(menu, mkrect(c.x, c.y + 60, c.w, 20), visible[i]->name, &font_small, t->text);
    }
    /* recommended files */
    draw_text(menu, 36, 322, "Recommended", &font_bold, t->text);
    int shown = 0;
    for (int i = 0; i < FS_MAX_FILES && shown < 4; i++) {
        file_t *f = fs_list(i);
        if (!f) break;
        if (qlen && !str_contains_ci(f->name, query)) continue;
        rect_t c = file_cell(shown);
        recommended[shown++] = i;
        if (menu_hover == 100 + shown - 1) fill_round_rect(menu, c, 10, t->hover);
        draw_app_icon(menu, c.x + 10, c.y + 10, 32, ICON_NOTES);
        draw_text(menu, c.x + 54, c.y + 8, f->name, &font_ui, t->text);
        char sz[32];
        ksnprintf(sz, sizeof sz, "%u bytes - in memory", (unsigned)f->size);
        draw_text(menu, c.x + 54, c.y + 28, sz, &font_small, t->text_dim);
    }
    nrecommended = shown;
    /* footer */
    fill_rect(menu, mkrect(0, H - 64, MENU_W, 64), t->dark ? 0x40000000 : 0x14000000);
    fill_rect(menu, mkrect(0, H - 64, MENU_W, 1), t->border);
    fill_circle(menu, 48, H - 32, 16, t->accent);
    draw_text_centered(menu, mkrect(32, H - 48, 32, 32), "G", &font_bold, 0xFFFFFFFF);
    draw_text(menu, 74, H - 32 - font_ui.height / 2, "Guest", &font_ui, t->text);
    rect_t pb = power_btn();
    if (menu_hover == 200 || menu_power) fill_round_rect(menu, pb, 8, t->hover);
    {
        int cx = pb.x + pb.w / 2, cy = pb.y + pb.h / 2;
        for (int deg = 50; deg <= 310; deg += 10) {
            float a0 = deg * 0.0174533f, c, sn;
            __asm__("fsincos" : "=t"(c), "=u"(sn) : "0"(a0));
            fill_circle(menu, cx + (int)(sn * 8), cy - (int)(c * 8), 1, t->text);
        }
        fill_round_rect(menu, mkrect(cx - 1, cy - 11, 3, 10), 1, t->text);
    }
    if (menu_power) {
        rect_t pop = mkrect(MENU_W - 210, H - 160, 200, 98);
        draw_shadow(menu, pop, 8, 90);
        fill_round_rect(menu, pop, 8, t->dark ? 0xFF2C2C32 : 0xFFFFFFFF);
        stroke_round_rect(menu, pop, 8, t->border);
        const char *items[2] = {"Restart", "Shut down"};
        for (int i = 0; i < 2; i++) {
            rect_t r = power_item(i);
            if (menu_hover == 201 + i) fill_round_rect(menu, r, 6, t->hover);
            draw_text(menu, r.x + 14, r.y + (r.h - font_ui.height) / 2, items[i], &font_ui, t->text);
        }
    }
    menu_dirty = 0;
}

static float menu_k(uint32_t now) { return anim_value(&menu_anim, now); }
static rect_t menu_rect_now(uint32_t now) {
    rect_t b = menu_rect_base();
    float k = menu_k(now);
    b.y += (int)((1 - k) * 48);
    return b;
}
int shell_start_open(void) { return menu_open; }

void shell_toggle_start(void) {
    float cur = menu_k(ticks_ms());
    menu_open = !menu_open;
    if (menu_open) {
        qlen = 0; query[0] = 0; menu_sel = 0; menu_power = 0; menu_hover = -1;
        filter_apps();
        menu_dirty = 1;
        anim_start(&menu_anim, cur, 1, 320, EASE_OUT_QUINT);
    } else {
        anim_start(&menu_anim, cur, 0, 180, EASE_OUT_CUBIC);
    }
    if (!menu_anim.active) menu_anim.to = menu_open;
}

static int menu_hit(int lx, int ly) {
    if (menu_power) {
        for (int i = 0; i < 2; i++) if (rect_contains(power_item(i), lx, ly)) return 201 + i;
    }
    if (rect_contains(power_btn(), lx, ly)) return 200;
    for (int i = 0; i < nvisible; i++) if (rect_contains(app_cell(i), lx, ly)) return i;
    for (int i = 0; i < nrecommended; i++) if (rect_contains(file_cell(i), lx, ly)) return 100 + i;
    return -1;
}

static void menu_activate(int h) {
    if (h >= 0 && h < nvisible) { shell_toggle_start(); wm_open(visible[h]); }
    else if (h >= 100 && h < 104) { file_t *f = fs_list(recommended[h - 100]); shell_toggle_start(); if (f) wm_open_file_in_notes(f->name); }
    else if (h == 200) { menu_power = !menu_power; menu_dirty = 1; }
    else if (h == 201) reboot();
    else if (h == 202) power_off();
}

/* ---- toasts ---- */
static surface_t *toast_surf;
static anim_t toast_anim;
static uint32_t toast_until;
static int toast_shown;
static rect_t toast_last;
#define TOAST_W 360
#define TOAST_H 84
void toast(const char *title, const char *body) {
    if (!toast_surf) toast_surf = surface_new(TOAST_W, TOAST_H);
    theme_t *t = &g_theme;
    rect_t at = mkrect(SCR_W - TOAST_W - 16, SCR_H - TASKBAR_H - TOAST_H - 16, TOAST_W, TOAST_H);
    clip_reset(toast_surf);
    acrylic(toast_surf, at, 0, 0, t->menu_tint);
    fill_rect(toast_surf, mkrect(0, 0, 4, TOAST_H), t->accent);
    draw_start_glyph(toast_surf, mkrect(14, 14, 32, 32));
    draw_text(toast_surf, 58, 16, title, &font_bold, t->text);
    draw_text(toast_surf, 58, 40, body, &font_small, t->text_dim);
    anim_start(&toast_anim, 0, 1, 520, EASE_SPRING);
    toast_until = ticks_ms() + 5000;
    toast_shown = 1;
}
static rect_t toast_rect_now(uint32_t now) {
    float k = anim_value(&toast_anim, now);
    int off = (int)((1 - k) * (TOAST_W + 24));
    return mkrect(SCR_W - TOAST_W - 16 + off, SCR_H - TASKBAR_H - TOAST_H - 16, TOAST_W, TOAST_H);
}

/* ---- compose / input / tick ---- */
void shell_compose(void) {
    uint32_t now = ticks_ms();
    draw_taskbar(now);
    if (toast_shown) {
        rect_t r = toast_rect_now(now);
        draw_shadow_ex(g_screen, mkrect(r.x + 2, r.y + 8, r.w - 4, r.h - 2), 12, 120, r);
        blit_rounded(g_screen, r.x, r.y, toast_surf, 10, 255);
    }
    float k = menu_k(now);
    if (k > 0.004f && menu) {
        rect_t r = menu_rect_now(now);
        int a = (int)(255 * CLAMP(k, 0, 1));
        draw_shadow_ex(g_screen, mkrect(r.x + 2, r.y + 10, r.w - 4, r.h - 2), 14, 150 * a / 255, a >= 250 ? r : mkrect(0, 0, 0, 0));
        blit_rounded(g_screen, r.x, r.y, menu, 12, a);
        stroke_round_rect(g_screen, r, 12, WITH_ALPHA(g_theme.dark ? 0xFFFFFF : 0x000000, 40 * a / 255));
    }
}

static rect_t pad(rect_t r, int p) { return mkrect(r.x - p, r.y - p, r.w + 2 * p, r.h + 2 * p + 8); }

int shell_mouse(int x, int y, int buttons, int ev, int wheel) {
    (void)wheel;
    uint32_t now = ticks_ms();
    /* start menu */
    if (menu_open) {
        rect_t mr = menu_rect_now(now);
        int inside = rect_contains(mr, x, y);
        int h = inside ? menu_hit(x - mr.x, y - mr.y) : -1;
        if (h != menu_hover) { menu_hover = h; menu_dirty = 1; }
        if (ev == ME_DOWN && inside) { menu_pressed = h; menu_dirty = 1; return 1; }
        if (ev == ME_UP && menu_pressed >= 0) {
            int p = menu_pressed;
            menu_pressed = -1;
            menu_dirty = 1;
            if (p == h) menu_activate(h);
            return 1;
        }
        if (inside) return 1;
        if (ev == ME_DOWN && !rect_contains(slot_rect(0), x, y)) {
            shell_toggle_start();
            if (y < SCR_H - TASKBAR_H) return 1;
        }
    }
    /* toast */
    if (toast_shown && ev == ME_DOWN && rect_contains(toast_rect_now(now), x, y)) {
        anim_start(&toast_anim, anim_value(&toast_anim, now), 0, 220, EASE_OUT_CUBIC);
        toast_until = 0;
        return 1;
    }
    /* taskbar */
    int hov = -1;
    if (y >= SCR_H - TASKBAR_H)
        for (int i = 0; i < nslots; i++) if (rect_contains(slot_rect(i), x, y)) hov = i;
    if (hov != slot_hovered) {
        if (slot_hovered >= 0) anim_start(&slot_hover[slot_hovered], anim_value(&slot_hover[slot_hovered], now), 0, 220, EASE_OUT_CUBIC);
        if (hov >= 0) anim_start(&slot_hover[hov], anim_value(&slot_hover[hov], now), 1, 140, EASE_OUT_CUBIC);
        slot_hovered = hov;
        wm_damage(taskbar_rect());
    }
    if (y < SCR_H - TASKBAR_H) { if (ev == ME_UP && slot_pressed >= 0) { slot_pressed = -1; wm_damage(taskbar_rect()); } return 0; }
    if (ev == ME_DOWN && (buttons & MB_LEFT)) { slot_pressed = hov; wm_damage(taskbar_rect()); }
    if (ev == ME_UP) {
        int p = slot_pressed;
        slot_pressed = -1;
        wm_damage(taskbar_rect());
        if (p >= 0 && p == hov) {
            if (p == 0) shell_toggle_start();
            else {
                const app_t *app = slot_app(p);
                window_t *w = top_window_of(app, 0);
                if (menu_open) shell_toggle_start();
                if (!w) wm_open(app);
                else {
                    extern void wm_taskbar_click(window_t *w);
                    wm_taskbar_click(w);
                }
            }
        }
    }
    return 1;
}

int shell_key(key_event_t *e) {
    if (!menu_open) return 0;
    int k = e->key;
    if (k == KEY_ESC) { if (menu_power) { menu_power = 0; menu_dirty = 1; } else shell_toggle_start(); return 1; }
    if (k == KEY_ENTER) { if (nvisible) menu_activate(menu_sel); else if (nrecommended) menu_activate(100); return 1; }
    if (k == KEY_BACKSPACE) { if (qlen) query[--qlen] = 0; menu_sel = 0; filter_apps(); menu_dirty = 1; return 1; }
    if (k == KEY_LEFT && menu_sel > 0) { menu_sel--; menu_dirty = 1; return 1; }
    if (k == KEY_RIGHT && menu_sel < nvisible - 1) { menu_sel++; menu_dirty = 1; return 1; }
    if (k == KEY_DOWN && menu_sel + 6 < nvisible) { menu_sel += 6; menu_dirty = 1; return 1; }
    if (k == KEY_UP && menu_sel >= 6) { menu_sel -= 6; menu_dirty = 1; return 1; }
    if (k >= 32 && k < 127 && !(e->mods & (MOD_CTRL | MOD_ALT)) && qlen < (int)sizeof query - 1) {
        query[qlen++] = (char)k;
        query[qlen] = 0;
        menu_sel = 0;
        filter_apps();
        menu_dirty = 1;
        return 1;
    }
    return 1; /* swallow everything else while Start is open */
}

static int menu_blink;
void shell_tick(uint32_t now) {
    /* clock */
    if (now - last_rtc > 1000 || last_min < 0) {
        last_rtc = now;
        rtc_time_t t;
        rtc_read(&t);
        if (t.min != last_min) {
            last_min = t.min;
            int h12 = t.hour % 12 ? t.hour % 12 : 12;
            ksnprintf(clock_txt, sizeof clock_txt, "%d:%02d %s", h12, t.min, t.hour < 12 ? "AM" : "PM");
            ksnprintf(date_txt, sizeof date_txt, "%d/%d/%d", t.mon, t.day, t.year);
            wm_damage(taskbar_rect());
        }
    }
    /* taskbar animations */
    int anim = 0;
    for (int i = 0; i < nslots; i++) if (slot_hover[i].active || slot_ind[i].active) anim = 1;
    if (anim) wm_damage(taskbar_rect());
    /* start menu */
    if (menu_open) {
        int b = (now / 530) % 2;
        if (b != menu_blink && qlen) { menu_blink = b; menu_dirty = 1; }
    }
    static int menu_was_animating;
    int need = 0;
    if (menu_dirty && (menu_open || menu_anim.active)) { render_menu(); need = 1; }
    if (menu_anim.active || menu_was_animating) need = 1;
    menu_was_animating = menu_anim.active;
    if (need) {
        float k = menu_k(now);
        rect_t r = k > 0.004f ? pad(menu_rect_now(now), 26) : mkrect(0, 0, 0, 0);
        wm_damage(menu_last);
        wm_damage(r);
        menu_last = r;
    }
    /* toast */
    if (toast_shown) {
        if (toast_until && now > toast_until) {
            toast_until = 0;
            anim_start(&toast_anim, anim_value(&toast_anim, now), 0, 260, EASE_OUT_CUBIC);
        }
        rect_t r = pad(toast_rect_now(now), 26);
        if (toast_anim.active) { wm_damage(toast_last); wm_damage(r); }
        toast_last = r;
        if (!toast_anim.active && !toast_until && anim_value(&toast_anim, now) <= 0) { toast_shown = 0; wm_damage(toast_last); }
    }
}

void shell_init(void) {
    nslots = 1 + g_app_count;
    shell_generate_wallpaper(0);
}
