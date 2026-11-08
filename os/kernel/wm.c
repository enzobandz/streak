/* Compositing window manager: window lifecycle, damage tracking, animated
 * open/close/minimise/maximise/snap, input routing, and the frame loop. */
#include "wm.h"

#define MAX_WIN 24
#define SHADOW_PAD 26
static window_t *zorder[MAX_WIN]; /* [0] bottom ... [n-1] top */
static int nwin, next_id = 1;
static window_t *focused;

/* damage */
#define MAX_DMG 24
static rect_t dmg[MAX_DMG];
static int ndmg;

/* pointer */
static int mx, my, mbuttons;
static int cur_drawn_x = -100, cur_drawn_y = -100, cur_kind, cur_drawn_kind;
enum { CUR_ARROW, CUR_EW, CUR_NS, CUR_NWSE, CUR_NESW };

/* interaction */
enum { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE, DRAG_CONTENT, DRAG_BUTTON };
static int drag_mode, drag_edges, drag_ox, drag_oy;
static rect_t drag_start_rect;
static window_t *drag_win;
static uint32_t last_click_ms;
static int last_click_x, last_click_y;

/* snap preview */
static int snap_target; /* 0 none, 1 max, 2 left, 3 right */
static rect_t snap_rect, snap_drawn;
static anim_t snap_anim;
static int snap_visible;

void wm_damage(rect_t r) {
    r = rect_intersect(r, mkrect(0, 0, SCR_W, SCR_H));
    if (rect_empty(r)) return;
    for (int i = 0; i < ndmg; i++) {
        rect_t u = rect_union(dmg[i], r);
        /* merge when the union is not much bigger than the parts */
        if ((long)u.w * u.h <= (long)dmg[i].w * dmg[i].h + (long)r.w * r.h + 4096) {
            dmg[i] = u;
            /* the grown rect may now overlap others */
            for (int j = 0; j < ndmg; j++) {
                if (j == i) continue;
                rect_t u2 = rect_union(dmg[i], dmg[j]);
                if ((long)u2.w * u2.h <= (long)dmg[i].w * dmg[i].h + (long)dmg[j].w * dmg[j].h + 4096) {
                    dmg[i] = u2;
                    dmg[j] = dmg[--ndmg];
                    if (i == ndmg) i = j;
                    j = -1;
                }
            }
            return;
        }
    }
    if (ndmg == MAX_DMG) {
        for (int i = 1; i < ndmg; i++) dmg[0] = rect_union(dmg[0], dmg[i]);
        ndmg = 1;
        dmg[0] = rect_union(dmg[0], r);
        return;
    }
    dmg[ndmg++] = r;
}
void wm_damage_all(void) { ndmg = 0; wm_damage(mkrect(0, 0, SCR_W, SCR_H)); }

static rect_t with_shadow(rect_t r) { return mkrect(r.x - SHADOW_PAD, r.y - SHADOW_PAD, r.w + 2 * SHADOW_PAD, r.h + 2 * SHADOW_PAD + 10); }
static rect_t work_area(void) { return mkrect(0, 0, SCR_W, SCR_H - TASKBAR_H); }
static rect_t win_rect(window_t *w) { return mkrect(w->x, w->y, w->w, w->h); }

rect_t wm_content_rect(window_t *w) { return mkrect(1, TITLE_H, w->w - 2, w->h - TITLE_H - 1); }
void wm_invalidate(window_t *w) { if (w) w->dirty = 1; }
void wm_set_title(window_t *w, const char *t) { strlcpy(w->title, t, sizeof w->title); w->dirty = 1; shell_windows_changed(); }
window_t *wm_focused(void) { return focused; }
int wm_window_count(void) { return nwin; }
window_t *wm_window_at(int i) { return i >= 0 && i < nwin ? zorder[i] : 0; }

static int zindex(window_t *w) {
    for (int i = 0; i < nwin; i++) if (zorder[i] == w) return i;
    return -1;
}

void wm_focus(window_t *w) {
    if (w && w->minimized) return;
    window_t *old = focused;
    focused = w;
    if (old && old != w) old->dirty = 1;
    if (w) {
        int i = zindex(w);
        if (i >= 0 && i != nwin - 1) {
            memmove(&zorder[i], &zorder[i + 1], (nwin - 1 - i) * sizeof(window_t *));
            zorder[nwin - 1] = w;
            wm_damage(with_shadow(win_rect(w)));
        }
        w->dirty = 1;
    }
    shell_windows_changed();
}

static void focus_top_visible(void) {
    for (int i = nwin - 1; i >= 0; i--)
        if (!zorder[i]->minimized && zorder[i]->state != WS_CLOSING) { wm_focus(zorder[i]); return; }
    focused = 0;
    shell_windows_changed();
}

/* Allocate the new surface before freeing the old one, so a failed
 * allocation leaves the window exactly as it was. Returns 0 on failure. */
static int resize_surface(window_t *w, int nw, int nh) {
    if (w->surf && w->surf->w == nw && w->surf->h == nh) return 1;
    surface_t *ns = surface_new(nw, nh);
    if (!ns) { klog("wm: out of memory for a %dx%d window\n", nw, nh); return 0; }
    surface_free(w->surf);
    w->surf = ns;
    w->w = nw;
    w->h = nh;
    w->dirty = 1;
    return 1;
}

static void start_anim(window_t *w, int state, rect_t from, rect_t to, float af, float at, uint32_t dur, int ease) {
    w->state = state;
    w->from = from;
    w->to = to;
    w->a_from = af;
    w->a_to = at;
    anim_start(&w->anim, 0, 1, dur, ease);
    if (!w->anim.active) w->anim.active = 0;
}

static rect_t scale_rect(rect_t r, float s) {
    int nw = (int)(r.w * s), nh = (int)(r.h * s);
    return mkrect(r.x + (r.w - nw) / 2, r.y + (r.h - nh) / 2, nw, nh);
}

window_t *wm_open(const app_t *app) {
    if (app->single)
        for (int i = 0; i < nwin; i++)
            if (zorder[i]->app == app && zorder[i]->state != WS_CLOSING) {
                window_t *w = zorder[i];
                if (w->minimized) {
                    w->minimized = 0;
                    start_anim(w, WS_RESTORING, shell_icon_rect_for(app), win_rect(w), 0, 255, 300, EASE_OUT_QUINT);
                }
                wm_focus(w);
                return w;
            }
    if (nwin >= MAX_WIN) { toast("Too many windows", "Close a window to open another one."); return 0; }
    window_t *w = kzalloc(sizeof *w);
    if (!w) { toast("Not enough memory", "Close some windows and try again."); return 0; }
    w->id = next_id++;
    w->app = app;
    strlcpy(w->title, app->name, sizeof w->title);
    int fw = MIN(app->w + 2, SCR_W - 40), fh = MIN(app->h + TITLE_H + 1, SCR_H - TASKBAR_H - 40);
    /* cascade new windows */
    static int cascade;
    rect_t wa = work_area();
    w->x = wa.x + (wa.w - fw) / 2 + (cascade % 6) * 28 - 70;
    w->y = wa.y + (wa.h - fh) / 2 + (cascade % 6) * 28 - 70;
    cascade++;
    w->x = CLAMP(w->x, 8, SCR_W - fw - 8);
    w->y = CLAMP(w->y, 8, wa.h - fh - 8);
    w->hover_btn = w->press_btn = -1;
    if (!resize_surface(w, fw, fh)) { kfree(w); toast("Not enough memory", "Close some windows and try again."); return 0; }
    if (app->init) app->init(w);
    if (app->init && !w->data) { /* every app allocates its state in init */
        surface_free(w->surf);
        kfree(w);
        toast("Not enough memory", "Close some windows and try again.");
        return 0;
    }
    zorder[nwin++] = w;
    start_anim(w, WS_OPENING, scale_rect(win_rect(w), 0.90f), win_rect(w), 0, 255, 280, EASE_OUT_QUINT);
    wm_focus(w);
    return w;
}

static void destroy(window_t *w) {
    int i = zindex(w);
    if (i < 0) return;
    wm_damage(with_shadow(w->last_drawn));
    memmove(&zorder[i], &zorder[i + 1], (nwin - 1 - i) * sizeof(window_t *));
    nwin--;
    if (w->app->close) w->app->close(w);
    if (drag_win == w) { drag_win = 0; drag_mode = DRAG_NONE; }
    surface_free(w->surf);
    kfree(w->data);
    kfree(w);
    if (focused == w) { focused = 0; focus_top_visible(); }
    shell_windows_changed();
}

void wm_close(window_t *w) {
    if (!w || w->state == WS_CLOSING) return;
    /* Destruction always happens later in wm_frame (even with animations off),
     * because wm_close is often called from inside the window's own handlers. */
    start_anim(w, WS_CLOSING, win_rect(w), scale_rect(win_rect(w), 0.92f), 255, 0, 170, EASE_OUT_CUBIC);
    if (drag_win == w) { drag_win = 0; drag_mode = DRAG_NONE; }
    if (focused == w) {
        focused = 0;
        for (int i = nwin - 1; i >= 0; i--)
            if (zorder[i] != w && !zorder[i]->minimized && zorder[i]->state != WS_CLOSING) { wm_focus(zorder[i]); break; }
    }
    shell_windows_changed();
}

static void minimize(window_t *w) {
    start_anim(w, WS_MINIMIZING, win_rect(w), shell_icon_rect_for(w->app), 255, 0, 300, EASE_OUT_QUINT);
    w->minimized = 1;
    if (focused == w) { focused = 0; focus_top_visible(); }
    shell_windows_changed();
}

static void restore_from_min(window_t *w) {
    w->minimized = 0;
    start_anim(w, WS_RESTORING, shell_icon_rect_for(w->app), win_rect(w), 0, 255, 300, EASE_OUT_QUINT);
    wm_focus(w);
}

static void morph_to(window_t *w, rect_t target) {
    rect_t from = win_rect(w);
    if (from.x == target.x && from.y == target.y && from.w == target.w && from.h == target.h) return;
    if (!resize_surface(w, target.w, target.h)) return;
    wm_damage(with_shadow(from));
    w->x = target.x;
    w->y = target.y;
    start_anim(w, WS_MORPH, from, target, 255, 255, 260, EASE_OUT_QUINT);
}

static void toggle_maximize(window_t *w) {
    if (!w->app->resizable) return;
    if (w->maximized || w->snapped) {
        w->maximized = w->snapped = 0;
        morph_to(w, w->restore);
    } else {
        w->restore = win_rect(w);
        w->maximized = 1;
        morph_to(w, work_area());
    }
}

static void snap(window_t *w, int target) {
    rect_t wa = work_area();
    if (!w->maximized && !w->snapped) w->restore = win_rect(w);
    if (target == 1) { w->maximized = 1; w->snapped = 0; morph_to(w, wa); return; }
    w->snapped = target;
    w->maximized = 0;
    if (target == 2) morph_to(w, mkrect(0, 0, wa.w / 2, wa.h));
    else morph_to(w, mkrect(wa.w / 2, 0, wa.w - wa.w / 2, wa.h));
}

/* ---- window frame painting ---- */
static rect_t btn_rect(window_t *w, int i) { return mkrect(w->w - 46 * (3 - i) - 1, 1, 46, TITLE_H - 1); } /* 0 min, 1 max, 2 close */

static void paint_window(window_t *w) {
    surface_t *s = w->surf;
    theme_t *t = &g_theme;
    clip_reset(s);
    int active = (w == focused);
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->win_bg);
    fill_rect(s, mkrect(0, 0, s->w, TITLE_H), t->title_bg);
    draw_app_icon(s, 12, (TITLE_H - 18) / 2, 18, w->app->icon);
    draw_text(s, 40, (TITLE_H - font_ui.height) / 2, w->title, &font_ui, active ? t->text : t->text_dim);
    for (int i = 0; i < 3; i++) {
        if (i == 1 && !w->app->resizable) continue;
        rect_t b = btn_rect(w, i);
        int hov = w->hover_btn == i, prs = w->press_btn == i && hov;
        uint32_t fg = active || hov ? t->text : t->text_dim;
        if (i == 2 && hov) { fill_rect(s, b, prs ? RGB(0x94, 0x1E, 0x14) : RGB(0xC4, 0x2B, 0x1C)); fg = 0xFFFFFFFF; }
        else if (hov) fill_rect(s, b, prs ? t->press : t->hover);
        int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
        if (i == 0) fill_rect(s, mkrect(cx - 5, cy, 10, 1), fg);
        else if (i == 1) {
            if (w->maximized) {
                uint32_t bgc = hov ? (prs ? t->press : t->hover) : 0;
                stroke_round_rect(s, mkrect(cx - 3, cy - 5, 8, 8), 2, fg);
                fill_rect(s, mkrect(cx - 5, cy - 3, 8, 8), t->title_bg);
                if (bgc) fill_rect(s, mkrect(cx - 5, cy - 3, 8, 8), bgc);
                stroke_round_rect(s, mkrect(cx - 5, cy - 3, 8, 8), 2, fg);
            } else stroke_round_rect(s, mkrect(cx - 5, cy - 5, 10, 10), 2, fg);
        } else {
            draw_line(s, cx - 5, cy - 5, cx + 5, cy + 5, 1, fg);
            draw_line(s, cx + 5, cy - 5, cx - 5, cy + 5, 1, fg);
        }
    }
    if (w->app->paint) {
        surface_t view = surface_view(s, wm_content_rect(w));
        w->app->paint(w, &view);
    }
    clip_reset(s);
    int rad = (w->maximized || w->snapped) ? 0 : WIN_RADIUS;
    if (rad) stroke_round_rect(s, mkrect(0, 0, s->w, s->h), rad, active ? WITH_ALPHA(t->accent, 0x90) : t->border);
    w->dirty = 0;
}

/* ---- compositor ---- */
static rect_t cur_rect_for(window_t *w, uint32_t now, int *alpha) {
    if (w->state == WS_NORMAL) { *alpha = 255; return win_rect(w); }
    float k = anim_value(&w->anim, now);
    rect_t a = w->from, b = w->to;
    *alpha = (int)(w->a_from + (w->a_to - w->a_from) * k);
    *alpha = CLAMP(*alpha, 0, 255);
    return mkrect(a.x + (int)((b.x - a.x) * k), a.y + (int)((b.y - a.y) * k),
                  a.w + (int)((b.w - a.w) * k), a.h + (int)((b.h - a.h) * k));
}

static void draw_window(window_t *w, uint32_t now) {
    if (w->minimized && w->state != WS_MINIMIZING) return;
    int alpha;
    rect_t r = cur_rect_for(w, now, &alpha);
    if (alpha <= 0 || r.w <= 0 || r.h <= 0) return;
    if (rect_empty(rect_intersect(with_shadow(r), g_clip))) return;
    int rad = (w->maximized || w->snapped) && w->state == WS_NORMAL ? 0 : WIN_RADIUS;
    int strength = (w == focused ? 150 : 90) * alpha / 255;
    if (!(w->maximized && w->state == WS_NORMAL))
        draw_shadow_ex(g_screen, mkrect(r.x + 2, r.y + 8, r.w - 4, r.h - 2), rad + 2, strength, alpha >= 250 ? r : mkrect(0, 0, 0, 0));
    if (w->state == WS_NORMAL || (r.w == w->surf->w && r.h == w->surf->h)) blit_rounded(g_screen, r.x, r.y, w->surf, rad, alpha);
    else blit_scaled(g_screen, r, w->surf, alpha, rad);
}

static const char *cursor_art[] = {
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.........X",
    "X..........X",
    "X......XXXXXX",
    "X...X..X",
    "X..XX..X",
    "X.X  X..X",
    "XX   X..X",
    "X     X..X",
    "      X..X",
    "       XX",
};
#define CUR_W 14
#define CUR_H 21
/* double-headed resize arrow centred on (x, y) along direction (dx, dy) */
static void draw_resize_cursor(int x, int y, int dx, int dy) {
    int L = 8, H = 4;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t c = pass ? 0xFFFFFFFF : RGB(20, 20, 20);
        int th = pass ? 2 : 4;
        draw_line(g_screen, x - dx * L, y - dy * L, x + dx * L, y + dy * L, th, c);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            int tx = x + sgn * dx * L, ty = y + sgn * dy * L;
            /* arrowhead: two short strokes back from the tip */
            int bx = -sgn * dx * H, by = -sgn * dy * H;
            draw_line(g_screen, tx, ty, tx + bx - by, ty + by + bx, th, c);
            draw_line(g_screen, tx, ty, tx + bx + by, ty + by - bx, th, c);
        }
    }
}

static void draw_cursor(int x, int y) {
    switch (cur_kind) {
    case CUR_EW: draw_resize_cursor(x, y, 1, 0); return;
    case CUR_NS: draw_resize_cursor(x, y, 0, 1); return;
    case CUR_NWSE: draw_resize_cursor(x, y, 1, 1); return;
    case CUR_NESW: draw_resize_cursor(x, y, 1, -1); return;
    }
    /* soft shadow then outline + fill */
    for (int pass = 0; pass < 2; pass++)
        for (int r = 0; r < 20; r++)
            for (int c = 0; cursor_art[r][c]; c++) {
                char ch = cursor_art[r][c];
                if (ch == ' ') continue;
                int px = x + c + (pass ? 0 : 1), py = y + r + (pass ? 0 : 1);
                if (!rect_contains(g_clip, px, py) || px < 0 || py < 0 || px >= SCR_W || py >= SCR_H) continue;
                uint32_t *p = &g_screen->px[py * g_screen->stride + px];
                if (!pass) *p = blend(*p, 0x50000000, 255);
                else *p = ch == 'X' ? RGB(20, 20, 20) : 0xFFFFFFFF;
            }
}

static void compose(uint32_t now) {
    for (int d = 0; d < ndmg; d++) {
        g_clip = dmg[d];
        shell_compose_wallpaper(dmg[d]);
        for (int i = 0; i < nwin; i++) draw_window(zorder[i], now);
        if (snap_visible) {
            float k = anim_value(&snap_anim, now);
            rect_t r = snap_rect;
            int inset = (int)(16 * (1 - k));
            rect_t pr = mkrect(r.x + 8 + inset, r.y + 8 + inset, r.w - 16 - 2 * inset, r.h - 16 - 2 * inset);
            fill_round_rect(g_screen, pr, 12, WITH_ALPHA(g_theme.accent, (int)(70 * k)));
            stroke_round_rect(g_screen, pr, 12, WITH_ALPHA(g_theme.accent, (int)(200 * k)));
        }
        shell_compose();
        draw_cursor(mx, my);
    }
    clip_reset(g_screen);
    for (int d = 0; d < ndmg; d++) display_flush(dmg[d]);
    ndmg = 0;
}

/* ---- input ---- */
static window_t *window_at(int x, int y) {
    for (int i = nwin - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (w->minimized || w->state == WS_CLOSING) continue;
        rect_t r = win_rect(w);
        if (w->app->resizable && !w->maximized) r = mkrect(r.x - 5, r.y - 5, r.w + 10, r.h + 10);
        if (rect_contains(r, x, y)) return w;
    }
    return 0;
}

static int edges_at(window_t *w, int x, int y) {
    if (!w->app->resizable || w->maximized) return 0;
    int e = 0, m = 6;
    if (x < w->x + m) e |= 1;
    if (x >= w->x + w->w - m) e |= 2;
    if (y < w->y + m - 2) e |= 4;
    if (y >= w->y + w->h - m) e |= 8;
    return e;
}

static void set_hover_btn(window_t *w, int b) {
    if (w && w->hover_btn != b) { w->hover_btn = b; w->dirty = 1; }
}

static void update_snap_preview(int target) {
    if (target == snap_target) return;
    snap_target = target;
    if (snap_visible) wm_damage(snap_drawn);
    snap_visible = target != 0;
    if (!target) return;
    rect_t wa = work_area();
    snap_rect = target == 1 ? wa : target == 2 ? mkrect(0, 0, wa.w / 2, wa.h) : mkrect(wa.w / 2, 0, wa.w - wa.w / 2, wa.h);
    snap_drawn = snap_rect;
    anim_start(&snap_anim, 0, 1, 220, EASE_OUT_QUINT);
    wm_damage(snap_rect);
}

static void move_window(window_t *w, int nx, int ny) {
    wm_damage(with_shadow(win_rect(w)));
    w->x = CLAMP(nx, 100 - w->w, SCR_W - 100); /* always leave part of the window grabbable */
    w->y = CLAMP(ny, 0, SCR_H - TASKBAR_H - TITLE_H);
    wm_damage(with_shadow(win_rect(w)));
}

static void handle_mouse_event(int ev, int wheel, int changed) {
    (void)changed;
    /* active drags first */
    if (drag_mode == DRAG_MOVE && drag_win) {
        if (ev == ME_MOVE) {
            window_t *w = drag_win;
            if (w->maximized || w->snapped) {
                /* pulling a maximised window off the top restores it under the cursor */
                if (ABS(my - drag_oy - w->y) < 8 && ABS(mx - drag_ox - w->x) < 8) return;
                rect_t rs = w->restore;
                float rel = (float)(mx - w->x) / w->w;
                wm_damage(with_shadow(win_rect(w)));
                if (!resize_surface(w, rs.w, rs.h)) return;
                w->maximized = w->snapped = 0;
                w->x = mx - (int)(rel * rs.w);
                w->y = my - drag_oy;
                drag_ox = mx - w->x;
            }
            move_window(w, mx - drag_ox, my - drag_oy);
            int t = 0;
            if (my <= 1) t = 1;
            else if (mx <= 1) t = 2;
            else if (mx >= SCR_W - 2) t = 3;
            if (!w->app->resizable) t = 0;
            update_snap_preview(t);
        } else if (ev == ME_UP) {
            if (snap_target) snap(drag_win, snap_target);
            update_snap_preview(0);
            drag_mode = DRAG_NONE;
            drag_win = 0;
        }
        return;
    }
    if (drag_mode == DRAG_RESIZE && drag_win) {
        if (ev == ME_MOVE) {
            window_t *w = drag_win;
            rect_t r = drag_start_rect;
            int dx = mx - drag_ox, dy = my - drag_oy;
            int minw = 260, minh = 180;
            if (drag_edges & 2) r.w = MAX(minw, r.w + dx);
            if (drag_edges & 8) r.h = MAX(minh, r.h + dy);
            if (drag_edges & 1) { int nw = MAX(minw, r.w - dx); r.x += r.w - nw; r.w = nw; }
            if (drag_edges & 4) { int nh = MAX(minh, r.h - dy); r.y += r.h - nh; r.h = nh; }
            if (r.y < 0) { r.h += r.y; r.y = 0; } /* keep the title bar on screen */
            if (r.h < minh) return;
            wm_damage(with_shadow(win_rect(w)));
            if (!resize_surface(w, r.w, r.h)) return;
            w->x = r.x; w->y = r.y;
            wm_damage(with_shadow(win_rect(w)));
        } else if (ev == ME_UP) { drag_mode = DRAG_NONE; drag_win = 0; }
        return;
    }
    if (drag_mode == DRAG_CONTENT && drag_win) {
        window_t *w = drag_win;
        rect_t c = wm_content_rect(w);
        if (w->app->mouse) w->app->mouse(w, mx - w->x - c.x, my - w->y - c.y, mbuttons, ev, wheel);
        if (ev == ME_UP) { drag_mode = DRAG_NONE; drag_win = 0; }
        return;
    }
    if (drag_mode == DRAG_BUTTON && drag_win) {
        window_t *w = drag_win;
        int b = -1;
        for (int i = 0; i < 3; i++) if (rect_contains(btn_rect(w, i), mx - w->x, my - w->y)) b = i;
        set_hover_btn(w, b);
        if (ev == ME_UP) {
            int pb = w->press_btn;
            w->press_btn = -1;
            w->dirty = 1;
            drag_mode = DRAG_NONE;
            drag_win = 0;
            if (b == pb) {
                if (b == 0) minimize(w);
                else if (b == 1) toggle_maximize(w);
                else if (b == 2) wm_close(w);
            }
        }
        return;
    }

    if (shell_mouse(mx, my, mbuttons, ev, wheel)) {
        for (int i = 0; i < nwin; i++) set_hover_btn(zorder[i], -1);
        return;
    }

    window_t *w = window_at(mx, my);
    for (int i = 0; i < nwin; i++) if (zorder[i] != w) set_hover_btn(zorder[i], -1);
    if (!w) return;
    int lx = mx - w->x, ly = my - w->y;
    int btn = -1;
    for (int i = 0; i < 3; i++)
        if (rect_contains(btn_rect(w, i), lx, ly) && (i != 1 || w->app->resizable)) btn = i;
    set_hover_btn(w, btn);
    rect_t c = wm_content_rect(w);

    if (ev == ME_DOWN) {
        if (w != focused) wm_focus(w);
        int e = edges_at(w, mx, my);
        if (e && (mbuttons & MB_LEFT)) {
            drag_mode = DRAG_RESIZE; drag_edges = e; drag_win = w;
            drag_ox = mx; drag_oy = my; drag_start_rect = win_rect(w);
            return;
        }
        if (btn >= 0) { drag_mode = DRAG_BUTTON; drag_win = w; w->press_btn = btn; w->dirty = 1; return; }
        if (ly < TITLE_H && ly >= 0 && (mbuttons & MB_LEFT)) {
            uint32_t now = ticks_ms();
            if (now - last_click_ms < 400 && ABS(mx - last_click_x) < 5 && ABS(my - last_click_y) < 5) {
                toggle_maximize(w);
                last_click_ms = 0;
                return;
            }
            last_click_ms = now; last_click_x = mx; last_click_y = my;
            drag_mode = DRAG_MOVE; drag_win = w; drag_ox = lx; drag_oy = ly;
            return;
        }
        if (rect_contains(c, lx, ly)) {
            drag_mode = DRAG_CONTENT; drag_win = w;
            if (w->app->mouse) w->app->mouse(w, lx - c.x, ly - c.y, mbuttons, ME_DOWN, 0);
        }
        return;
    }
    if (rect_contains(c, lx, ly) && w->app->mouse && (ev == ME_MOVE || ev == ME_WHEEL))
        w->app->mouse(w, lx - c.x, ly - c.y, mbuttons, ev, wheel);
}

static int accel(int d) {
    int a = ABS(d);
    if (a > 10) return d * 2;
    if (a > 4) return d * 3 / 2;
    return d;
}

static void process_input(void) {
    mouse_event_t me;
    int moved = 0;
    while (mouse_poll(&me)) {
        int ox = mx, oy = my;
        if (me.absolute) {
            mx = (int)((uint32_t)me.ax * SCR_W / 65536);
            my = (int)((uint32_t)me.ay * SCR_H / 65536);
        } else {
            mx += accel(me.dx);
            my += accel(me.dy);
        }
        mx = CLAMP(mx, 0, SCR_W - 1);
        my = CLAMP(my, 0, SCR_H - 1);
        int pressed = me.buttons & ~mbuttons, released = mbuttons & ~me.buttons;
        if (mx != ox || my != oy) { moved = 1; handle_mouse_event(ME_MOVE, 0, 0); }
        mbuttons = me.buttons;
        if (pressed) handle_mouse_event(ME_DOWN, 0, pressed);
        if (released) handle_mouse_event(ME_UP, 0, released);
        if (me.wheel) handle_mouse_event(ME_WHEEL, me.wheel, 0);
    }
    /* pick the pointer shape: resize arrows over (or while dragging) window edges */
    int edges = 0;
    if (drag_mode == DRAG_RESIZE) edges = drag_edges;
    else if (drag_mode == DRAG_NONE && !shell_start_open() && my < SCR_H - TASKBAR_H) {
        window_t *hw = window_at(mx, my);
        if (hw) edges = edges_at(hw, mx, my);
    }
    int kind = CUR_ARROW;
    if ((edges & 3) && (edges & 12)) kind = ((edges & 1) != 0) == ((edges & 4) != 0) ? CUR_NWSE : CUR_NESW;
    else if (edges & 3) kind = CUR_EW;
    else if (edges & 12) kind = CUR_NS;
    if (moved || cur_drawn_x != mx || cur_drawn_y != my || kind != cur_drawn_kind) {
        cur_kind = kind;
        /* one box covers every cursor shape: the arrow extends down-right, resize arrows are centred */
        wm_damage(mkrect(cur_drawn_x - 14, cur_drawn_y - 14, CUR_W + 30, CUR_H + 30));
        wm_damage(mkrect(mx - 14, my - 14, CUR_W + 30, CUR_H + 30));
        cur_drawn_x = mx;
        cur_drawn_y = my;
        cur_drawn_kind = kind;
    }

    key_event_t ke;
    while (keyboard_poll(&ke)) {
        if (ke.key == KEY_SUPER) { shell_toggle_start(); continue; }
        if (shell_key(&ke)) continue;
        if ((ke.mods & MOD_ALT) && ke.key == KEY_TAB) {
            /* cycle: bring the bottom-most window to the front */
            for (int i = 0; i < nwin; i++) {
                window_t *w = zorder[i];
                if (w->state == WS_CLOSING) continue;
                if (w->minimized) restore_from_min(w); else wm_focus(w);
                break;
            }
            continue;
        }
        if ((ke.mods & MOD_ALT) && ke.key == KEY_F4) { if (focused) wm_close(focused); continue; }
        if ((ke.mods & MOD_CTRL) && (ke.mods & MOD_ALT) && (ke.key == 't' || ke.key == 'T')) { wm_open(&app_terminal); continue; }
        if (focused && (ke.mods & MOD_ALT) && (ke.key == KEY_UP || ke.key == KEY_DOWN || ke.key == KEY_LEFT || ke.key == KEY_RIGHT)) {
            if (ke.key == KEY_UP) snap(focused, 1);
            else if (ke.key == KEY_LEFT) snap(focused, 2);
            else if (ke.key == KEY_RIGHT) snap(focused, 3);
            else if (focused->maximized || focused->snapped) toggle_maximize(focused);
            else minimize(focused);
            continue;
        }
        if (focused && focused->app->key) focused->app->key(focused, &ke);
    }
}

/* ---- frame ---- */
void wm_frame(uint32_t now) {
    process_input();
    shell_tick(now);
    for (int i = 0; i < nwin; i++) {
        window_t *w = zorder[i];
        if (w->app->tick && !w->minimized) w->app->tick(w, now);
    }
    for (int i = 0; i < nwin; i++) {
        window_t *w = zorder[i];
        if (w->dirty) {
            paint_window(w);
            if (!w->minimized) wm_damage(with_shadow(win_rect(w)));
        }
        if (w->state != WS_NORMAL) {
            int a;
            rect_t r = cur_rect_for(w, now, &a);
            wm_damage(with_shadow(w->last_drawn));
            wm_damage(with_shadow(r));
            w->last_drawn = r;
            if (!w->anim.active) {
                int st = w->state;
                w->state = WS_NORMAL;
                wm_damage(with_shadow(win_rect(w)));
                if (st == WS_CLOSING) { destroy(w); i--; continue; }
                if (st == WS_MINIMIZING) wm_damage(with_shadow(r));
            }
        } else {
            w->last_drawn = win_rect(w);
        }
    }
    if (snap_visible && snap_anim.active) wm_damage(snap_rect);
    compose(now);
}

void wm_apply_theme(void) {
    extern void shell_invalidate_menu(void);
    shell_invalidate_menu();
    for (int i = 0; i < nwin; i++) zorder[i]->dirty = 1;
    wm_damage_all();
}

void wm_init(void) {
    mx = SCR_W / 2;
    my = SCR_H / 2;
    wm_damage_all();
}

/* Shut down or restart: let every app save (Notes autosaves on close), show a
 * goodbye screen, then switch off. If the firmware ignores the request, say so. */
void wm_shutdown(int restart) {
    for (int i = nwin - 1; i >= 0; i--)
        if (zorder[i]->app->close) { zorder[i]->app->close(zorder[i]); }
    clip_reset(g_screen);
    fill_rect(g_screen, mkrect(0, 0, SCR_W, SCR_H), RGB(0x07, 0x08, 0x10));
    draw_text_centered(g_screen, mkrect(0, SCR_H / 2 - 30, SCR_W, 40), restart ? "Restarting..." : "Shutting down...", &font_title, 0xFFFFFFFF);
    display_flush(mkrect(0, 0, SCR_W, SCR_H));
    if (restart) reboot();
    power_off();
    fill_rect(g_screen, mkrect(0, 0, SCR_W, SCR_H), RGB(0x07, 0x08, 0x10));
    draw_text_centered(g_screen, mkrect(0, SCR_H / 2 - 30, SCR_W, 40), "It's now safe to turn off your computer.", &font_title, 0xFFFFFFFF);
    display_flush(mkrect(0, 0, SCR_W, SCR_H));
    for (;;) { cli(); hlt(); }
}

/* Shown on CPU exceptions instead of silently freezing. */
void panic_screen(const char *msg, regs_t *r) {
    if (!g_screen) return;
    clip_reset(g_screen);
    fill_rect(g_screen, mkrect(0, 0, SCR_W, SCR_H), RGB(0x12, 0x1A, 0x3A));
    draw_text(g_screen, 80, 100, ":(", &font_big, 0xFFFFFFFF);
    draw_text(g_screen, 80, 180, "Fluvia ran into a problem and stopped.", &font_title, 0xFFFFFFFF);
    char buf[160];
    ksnprintf(buf, sizeof buf, "%s  (vector %u, error %x, at %x)", msg, r->vec, r->err, r->eip);
    draw_text(g_screen, 80, 230, buf, &font_mono, 0xFFDDDDFF);
    draw_text(g_screen, 80, 270, "Restart the machine to continue.", &font_ui, 0xFFCCCCDD);
    display_flush(mkrect(0, 0, SCR_W, SCR_H));
}

void wm_taskbar_click(window_t *w) {
    if (w->minimized) restore_from_min(w);
    else if (w == focused && w->state == WS_NORMAL) minimize(w);
    else wm_focus(w);
}
