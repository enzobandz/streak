/* Calculator with keyboard support and animated key presses. */
#include "../kernel/wm.h"

typedef struct {
    char disp[40], hist[64];
    double acc;
    int op, fresh, hover, pressed;
    anim_t flash[20];
} calc_t;
static const char *labels[20] = {"C", "+/-", "%", "/", "7", "8", "9", "x", "4", "5", "6", "-", "1", "2", "3", "+", "0", ".", "<", "="};
#define DISP_H 110

static void c_init(window_t *w) {
    calc_t *c = kzalloc(sizeof *c);
    w->data = c;
    if (!c) return;
    strcpy(c->disp, "0");
    c->fresh = 1;
    c->hover = c->pressed = -1;
}

static rect_t key_rect(window_t *w, int i) {
    int cw = w->w - 2, ch = w->h - TITLE_H - 1 - DISP_H;
    int g = 4, bw = (cw - 16 - 3 * g) / 4, bh = (ch - 12 - 4 * g) / 5;
    return mkrect(8 + (i % 4) * (bw + g), DISP_H + 4 + (i / 4) * (bh + g), bw, bh);
}

static double apply(double a, double b, int op) {
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case 'x': return a * b;
    case '/': return b == 0 ? 0.0 / 0.0 : a / b;
    }
    return b;
}

static void press(window_t *w, int i) {
    calc_t *c = w->data;
    anim_start(&c->flash[i], 1, 0, 350, EASE_OUT_CUBIC);
    const char *l = labels[i];
    int ok;
    double cur = parse_double(c->disp, &ok);
    if (!ok) cur = 0;
    char opch[2] = {0, 0};
    if (l[0] >= '0' && l[0] <= '9') {
        if (c->fresh || !strcmp(c->disp, "0")) { c->disp[0] = 0; c->fresh = 0; }
        if (strlen(c->disp) < 16) { size_t n = strlen(c->disp); c->disp[n] = l[0]; c->disp[n + 1] = 0; }
    } else if (!strcmp(l, ".")) {
        if (c->fresh) { strcpy(c->disp, "0"); c->fresh = 0; }
        if (!strchr(c->disp, '.')) { size_t n = strlen(c->disp); c->disp[n] = '.'; c->disp[n + 1] = 0; }
    } else if (!strcmp(l, "C")) {
        strcpy(c->disp, "0"); c->hist[0] = 0; c->acc = 0; c->op = 0; c->fresh = 1;
    } else if (!strcmp(l, "<")) {
        size_t n = strlen(c->disp);
        if (!c->fresh && n > 1) c->disp[n - 1] = 0; else { strcpy(c->disp, "0"); c->fresh = 1; }
        if (!strcmp(c->disp, "-")) { strcpy(c->disp, "0"); c->fresh = 1; }
    } else if (!strcmp(l, "+/-")) {
        fmt_double(c->disp, sizeof c->disp, -cur);
    } else if (!strcmp(l, "%")) {
        fmt_double(c->disp, sizeof c->disp, c->op ? c->acc * cur / 100 : cur / 100);
    } else if (!strcmp(l, "=")) {
        if (c->op) {
            char a[32], b[32];
            fmt_double(a, sizeof a, c->acc);
            fmt_double(b, sizeof b, cur);
            ksnprintf(c->hist, sizeof c->hist, "%s %c %s =", a, c->op, b);
            fmt_double(c->disp, sizeof c->disp, apply(c->acc, cur, c->op));
            c->op = 0;
        }
        c->fresh = 1;
    } else {
        if (c->op && !c->fresh) cur = apply(c->acc, cur, c->op);
        c->acc = cur;
        c->op = l[0];
        opch[0] = (char)c->op;
        char a[32];
        fmt_double(a, sizeof a, cur);
        fmt_double(c->disp, sizeof c->disp, cur);
        ksnprintf(c->hist, sizeof c->hist, "%s %s", a, opch);
        c->fresh = 1;
    }
    wm_invalidate(w);
}

static void c_paint(window_t *w, surface_t *s) {
    calc_t *c = w->data;
    theme_t *t = &g_theme;
    uint32_t now = ticks_ms();
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->win_bg);
    draw_text(s, s->w - 16 - text_width(c->hist, &font_ui), 14, c->hist, &font_ui, t->text_dim);
    const font_t *f = text_width(c->disp, &font_big) > s->w - 32 ? &font_title : &font_big;
    draw_text(s, s->w - 16 - text_width(c->disp, f), DISP_H - f->height - 4, c->disp, f, t->text);
    for (int i = 0; i < 20; i++) {
        rect_t r = key_rect(w, i);
        int op = (i % 4 == 3) || i < 3;
        uint32_t bg = i == 19 ? t->accent : op ? (t->dark ? RGB(0x2D, 0x2D, 0x33) : RGB(0xEC, 0xEC, 0xF0)) : t->field;
        if (c->hover == i) bg = i == 19 ? mix(bg, 0xFFFFFFFF, 30) : blend(bg, t->hover, 255);
        float fl = anim_value(&c->flash[i], now);
        if (fl > 0) bg = mix(bg, i == 19 ? 0xFFFFFFFF : t->accent, (int)(fl * (i == 19 ? 70 : 90)));
        fill_round_rect(s, r, 6, bg);
        if (i != 19) stroke_round_rect(s, r, 6, t->dark ? 0x14FFFFFF : 0x14000000);
        const char *l = labels[i];
        draw_text_centered(s, r, !strcmp(l, "<") ? "Del" : l, &font_bold, i == 19 ? 0xFFFFFFFF : t->text);
    }
}

static void c_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    (void)b; (void)wheel;
    calc_t *c = w->data;
    int h = -1;
    for (int i = 0; i < 20; i++) if (rect_contains(key_rect(w, i), x, y)) h = i;
    if (h != c->hover) { c->hover = h; wm_invalidate(w); }
    if (ev == ME_DOWN) c->pressed = h;
    if (ev == ME_UP) { if (h >= 0 && h == c->pressed) press(w, h); c->pressed = -1; }
}

static void c_key(window_t *w, key_event_t *e) {
    int k = e->key;
    const char *l = 0;
    char one[2] = {0, 0};
    if ((k >= '0' && k <= '9') || k == '.' || k == '+' || k == '-' || k == '/' || k == '%') { one[0] = (char)k; l = one; }
    else if (k == '*' || k == 'x' || k == 'X') l = "x";
    else if (k == KEY_ENTER || k == '=') l = "=";
    else if (k == KEY_BACKSPACE) l = "<";
    else if (k == KEY_ESC || k == 'c' || k == 'C' || k == KEY_DELETE) l = "C";
    if (!l) return;
    for (int i = 0; i < 20; i++) if (!strcmp(labels[i], l)) press(w, i);
}

static void c_tick(window_t *w, uint32_t now) {
    calc_t *c = w->data;
    (void)now;
    for (int i = 0; i < 20; i++) if (c->flash[i].active) { wm_invalidate(w); return; }
}

const app_t app_calc = {"Calculator", "Basic arithmetic", ICON_CALC, 340, 500, 1, 0,
                        c_init, c_paint, c_key, c_mouse, c_tick, 0};
