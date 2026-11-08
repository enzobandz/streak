/* Paint: smooth brush strokes on a canvas with a palette and brush sizes. */
#include "../kernel/wm.h"

#define CW 1600
#define CH 1000
#define TB 52
static const uint32_t palette[] = {
    RGB(0x1A, 0x1A, 0x1E), RGB(0xFF, 0xFF, 0xFF), RGB(0xFF, 0x4D, 0x6D), RGB(0xFF, 0x8A, 0x3D), RGB(0xFF, 0xC8, 0x3D),
    RGB(0x2E, 0xC4, 0x6B), RGB(0x1F, 0xB6, 0xC9), RGB(0x3D, 0x7B, 0xFF), RGB(0x8B, 0x5C, 0xF6), RGB(0xEC, 0x48, 0x99),
};
#define NPAL 10
static const int sizes[] = {3, 8, 18};
typedef struct { surface_t *canvas; int color, size, drawing, lx, ly, hover; } paint_t;

static rect_t swatch(int i) { return mkrect(14 + i * 32, 12, 26, 26); }
static rect_t size_btn(int i) { return mkrect(14 + NPAL * 32 + 16 + i * 36, 8, 34, 34); }
static rect_t clear_btn(int w) { return mkrect(w - 88, 9, 76, 32); }

static void p_init(window_t *w) {
    paint_t *p = kzalloc(sizeof *p);
    if (!p) return;
    p->canvas = surface_new(CW, CH);
    if (!p->canvas) { kfree(p); return; } /* w->data stays NULL: the WM reports out of memory */
    w->data = p;
    fill_rect(p->canvas, mkrect(0, 0, CW, CH), 0xFFFFFFFF);
    p->color = 7;
    p->size = 1;
    p->hover = -1;
}

static void p_paint(window_t *w, surface_t *s) {
    paint_t *p = w->data;
    theme_t *t = &g_theme;
    blit_opaque(s, 0, TB, p->canvas, mkrect(0, 0, MIN(s->w, CW), MIN(s->h - TB, CH)));
    if (s->h - TB > CH) fill_rect(s, mkrect(0, TB + CH, s->w, s->h - TB - CH), t->win_bg);
    if (s->w > CW) fill_rect(s, mkrect(CW, TB, s->w - CW, s->h - TB), t->win_bg);
    fill_rect(s, mkrect(0, 0, s->w, TB), t->title_bg);
    fill_rect(s, mkrect(0, TB - 1, s->w, 1), t->border);
    for (int i = 0; i < NPAL; i++) {
        rect_t r = swatch(i);
        if (i == p->color) { stroke_round_rect(s, mkrect(r.x - 3, r.y - 3, r.w + 6, r.h + 6), 16, t->accent); }
        fill_circle(s, r.x + r.w / 2, r.y + r.h / 2, i == p->color ? 11 : (p->hover == i ? 12 : 10), palette[i]);
        if (i <= 1) stroke_round_rect(s, mkrect(r.x + 2, r.y + 2, r.w - 4, r.h - 4), 11, t->border);
    }
    for (int i = 0; i < 3; i++) {
        rect_t r = size_btn(i);
        if (i == p->size) fill_round_rect(s, r, 8, WITH_ALPHA(t->accent, 0x40));
        else if (p->hover == 20 + i) fill_round_rect(s, r, 8, t->hover);
        fill_circle(s, r.x + r.w / 2, r.y + r.h / 2, MAX(2, sizes[i] / 2), t->text);
    }
    ui_button(s, clear_btn(s->w), "Clear", p->hover == 30, 0, 0);
}

static void stroke(paint_t *p, int x0, int y0, int x1, int y1) {
    clip_reset(p->canvas);
    draw_line(p->canvas, x0, y0, x1, y1, sizes[p->size], palette[p->color]);
}

static void p_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    (void)wheel;
    paint_t *p = w->data;
    int cw = w->w - 2;
    if (ev == ME_MOVE && !p->drawing) {
        int h = -1;
        for (int i = 0; i < NPAL; i++) if (rect_contains(swatch(i), x, y)) h = i;
        for (int i = 0; i < 3; i++) if (rect_contains(size_btn(i), x, y)) h = 20 + i;
        if (rect_contains(clear_btn(cw), x, y)) h = 30;
        if (h != p->hover) { p->hover = h; wm_invalidate(w); }
        return;
    }
    if (ev == ME_DOWN) {
        if (y < TB) {
            for (int i = 0; i < NPAL; i++) if (rect_contains(swatch(i), x, y)) p->color = i;
            for (int i = 0; i < 3; i++) if (rect_contains(size_btn(i), x, y)) p->size = i;
            if (rect_contains(clear_btn(cw), x, y)) fill_rect(p->canvas, mkrect(0, 0, CW, CH), 0xFFFFFFFF);
            wm_invalidate(w);
            return;
        }
        p->drawing = 1;
        p->lx = x;
        p->ly = y - TB;
        stroke(p, x, y - TB, x, y - TB);
        wm_invalidate(w);
    } else if (ev == ME_MOVE && p->drawing && (b & MB_LEFT)) {
        stroke(p, p->lx, p->ly, x, y - TB);
        p->lx = x;
        p->ly = y - TB;
        wm_invalidate(w);
    } else if (ev == ME_UP) p->drawing = 0;
}

static void p_close(window_t *w) { surface_free(((paint_t *)w->data)->canvas); }

const app_t app_paint = {"Paint", "Draw and sketch", ICON_PAINT, 820, 560, 1, 0,
                         p_init, p_paint, 0, p_mouse, 0, p_close};
