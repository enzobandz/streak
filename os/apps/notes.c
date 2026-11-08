/* Notes: a text editor backed by the file system. Supports selection with
 * Shift+arrows or the mouse, clipboard (Ctrl+A/C/X/V), undo/redo (Ctrl+Z/Y),
 * auto-indent and Ctrl+S to save. */
#include "../kernel/wm.h"

#define CAP 32768
#define UNDO 24
typedef struct { char *buf; int len, cur; } snap_t;
typedef struct {
    char name[FS_NAME_LEN];
    char buf[CAP];
    int len, cur, anchor;   /* anchor < 0: no selection */
    int top, left, follow;  /* first visible line, horizontal scroll (px), scroll to caret on next paint */
    int saved;
    uint32_t blink_at, saved_flash, last_edit_ms, last_click;
    int blink_on, last_kind, dragging;
    snap_t undo[UNDO], redo[UNDO];
    int nundo, nredo;
} notes_t;
#define BAR 40
#define PADX 16
#define LH (font_ui.height + 4)

static void snap_free(snap_t *s) { kfree(s->buf); s->buf = 0; }
static int snap_take(notes_t *n, snap_t *s) {
    s->buf = kmalloc(n->len + 1);
    if (!s->buf) return 0;
    memcpy(s->buf, n->buf, n->len);
    s->len = n->len;
    s->cur = n->cur;
    return 1;
}
static void snap_apply(notes_t *n, snap_t *s) {
    memcpy(n->buf, s->buf, s->len);
    n->len = s->len;
    n->buf[n->len] = 0;
    n->cur = MIN(s->cur, n->len);
    n->anchor = -1;
}
static void stack_push(snap_t *st, int *cnt, snap_t s) {
    if (*cnt == UNDO) { snap_free(&st[0]); memmove(st, st + 1, sizeof(snap_t) * (UNDO - 1)); (*cnt)--; }
    st[(*cnt)++] = s;
}
static void clear_redo(notes_t *n) { while (n->nredo) snap_free(&n->redo[--n->nredo]); }

/* Record an undo point before an edit. Consecutive edits of the same kind
 * within a second are grouped (typing a word is one undo step). */
enum { EK_TYPE = 1, EK_DELETE, EK_OTHER };
static void before_edit(notes_t *n, int kind) {
    uint32_t now = ticks_ms();
    if (kind == EK_OTHER || kind != n->last_kind || now - n->last_edit_ms > 1000) {
        snap_t s;
        if (snap_take(n, &s)) stack_push(n->undo, &n->nundo, s);
    }
    clear_redo(n);
    n->last_kind = kind;
    n->last_edit_ms = now;
    n->saved = 0;
}
static void undo(notes_t *n) {
    if (!n->nundo) return;
    snap_t s;
    if (snap_take(n, &s)) stack_push(n->redo, &n->nredo, s);
    snap_t u = n->undo[--n->nundo];
    snap_apply(n, &u);
    snap_free(&u);
    n->last_kind = 0;
    n->saved = 0;
}
static void redo(notes_t *n) {
    if (!n->nredo) return;
    snap_t s;
    if (snap_take(n, &s)) stack_push(n->undo, &n->nundo, s);
    snap_t r = n->redo[--n->nredo];
    snap_apply(n, &r);
    snap_free(&r);
    n->last_kind = 0;
    n->saved = 0;
}

static void load(notes_t *n, const char *name) {
    strlcpy(n->name, name, sizeof n->name);
    file_t *f = fs_find(name);
    n->len = f ? (int)MIN(f->size, CAP - 1) : 0;
    if (f) memcpy(n->buf, f->data, n->len);
    n->buf[n->len] = 0;
    n->cur = 0;
    n->anchor = -1;
    n->top = n->left = 0;
    n->saved = 1;
    while (n->nundo) snap_free(&n->undo[--n->nundo]);
    clear_redo(n);
}
static void save(window_t *w, notes_t *n) {
    if (fs_write(n->name, n->buf, n->len)) {
        n->saved = 1;
        n->saved_flash = ticks_ms();
    } else toast("Could not save", "The file system is full.");
    wm_invalidate(w);
}
static void retitle(window_t *w, notes_t *n) {
    char t[64];
    ksnprintf(t, sizeof t, "%s%s - Notes", n->saved ? "" : "* ", n->name);
    if (strcmp(t, w->title)) wm_set_title(w, t);
}

static void n_init(window_t *w) {
    notes_t *n = kzalloc(sizeof *n);
    w->data = n;
    if (!n) return;
    load(n, "notes.txt");
    retitle(w, n);
}

void wm_open_file_in_notes(const char *name) {
    /* reuse a window that already shows this file */
    for (int i = 0; i < wm_window_count(); i++) {
        window_t *w = wm_window_at(i);
        if (w->app == &app_notes && w->data && !strcmp(((notes_t *)w->data)->name, name) && w->state != WS_CLOSING) {
            if (w->minimized) wm_taskbar_click(w); else wm_focus(w);
            return;
        }
    }
    window_t *w = wm_open(&app_notes);
    if (!w || !w->data) return;
    load(w->data, name);
    retitle(w, w->data);
    wm_invalidate(w);
}

static int line_start(notes_t *n, int pos) { while (pos > 0 && n->buf[pos - 1] != '\n') pos--; return pos; }
static int line_end(notes_t *n, int pos) { while (pos < n->len && n->buf[pos] != '\n') pos++; return pos; }
static int line_of(notes_t *n, int pos) { int l = 0; for (int i = 0; i < pos; i++) if (n->buf[i] == '\n') l++; return l; }
static int pos_of_line(notes_t *n, int line) {
    int p = 0;
    while (line > 0 && p < n->len) { if (n->buf[p++] == '\n') line--; }
    return p;
}
static int has_sel(notes_t *n) { return n->anchor >= 0 && n->anchor != n->cur; }
static int sel_a(notes_t *n) { return MIN(n->anchor, n->cur); }
static int sel_b(notes_t *n) { return MAX(n->anchor, n->cur); }

static void n_paint(window_t *w, surface_t *s) {
    notes_t *n = w->data;
    theme_t *t = &g_theme;
    if (!n) return;
    uint32_t bg = t->dark ? RGB(0x1A, 0x1A, 0x1E) : RGB(0xFF, 0xFF, 0xFF);
    fill_rect(s, mkrect(0, 0, s->w, s->h), bg);
    int rows = MAX(1, (s->h - BAR - 12) / LH);
    int cl = line_of(n, n->cur);
    int ls = line_start(n, n->cur);
    int caret_x = text_width_n(n->buf + ls, n->cur - ls, &font_ui);
    int view_w = s->w - 2 * PADX;
    if (n->follow) {
        if (cl < n->top) n->top = cl;
        if (cl >= n->top + rows) n->top = cl - rows + 1;
        if (caret_x - n->left > view_w - 8) n->left = caret_x - view_w + 40;
        if (caret_x < n->left) n->left = MAX(0, caret_x - 40);
        n->follow = 0;
    }
    int total = line_of(n, n->len) + 1;
    n->top = CLAMP(n->top, 0, MAX(0, total - 1));
    int p = pos_of_line(n, n->top);
    int y = BAR + 10;
    int ln = n->top;
    surface_t text = surface_view(s, mkrect(PADX, BAR, view_w, s->h - BAR));
    int a = has_sel(n) ? sel_a(n) : -1, b = has_sel(n) ? sel_b(n) : -1;
    uint32_t selc = WITH_ALPHA(t->accent, w == wm_focused() ? 0x55 : 0x30);
    while (y + LH <= s->h + LH && p <= n->len) {
        int e = line_end(n, p);
        int ty = y - BAR;
        if (a >= 0 && a <= e && b >= p) {
            int s0 = MAX(a, p), s1 = MIN(b, e);
            int x0 = text_width_n(n->buf + p, s0 - p, &font_ui) - n->left;
            int x1 = text_width_n(n->buf + p, s1 - p, &font_ui) - n->left;
            if (b > e) x1 += 6; /* selection continues past the line break */
            fill_rect(&text, mkrect(x0, ty - 1, x1 - x0, LH), selc);
        }
        draw_text_n(&text, -n->left, ty, n->buf + p, e - p, &font_ui, t->text);
        if (ln == cl && n->blink_on && w == wm_focused())
            fill_rect(&text, mkrect(caret_x - n->left, ty, 2, font_ui.height), t->accent);
        if (e >= n->len) break;
        p = e + 1;
        y += LH;
        ln++;
    }
    /* scrollbar */
    if (total > rows) {
        int th = MAX(24, (s->h - BAR - 8) * rows / total);
        int ty = BAR + 4 + (s->h - BAR - 8 - th) * n->top / MAX(1, total - rows);
        fill_round_rect(s, mkrect(s->w - 7, ty, 4, th), 2, t->dark ? 0x50FFFFFF : 0x40000000);
    }
    /* toolbar */
    fill_rect(s, mkrect(0, 0, s->w, BAR), t->title_bg);
    fill_rect(s, mkrect(0, BAR - 1, s->w, 1), t->border);
    char info[48];
    ksnprintf(info, sizeof info, "%s   Ln %d, Col %d", n->name, cl + 1, n->cur - ls + 1);
    draw_text(s, PADX, (BAR - font_small.height) / 2, info, &font_small, t->text_dim);
    const char *st = n->saved ? "Saved" : "Ctrl+S to save";
    uint32_t sc = t->text_dim;
    if (n->saved && ticks_ms() - n->saved_flash < 1200) sc = t->accent;
    draw_text(s, s->w - text_width(st, &font_small) - PADX, (BAR - font_small.height) / 2, st, &font_small, sc);
}

static void delete_range(notes_t *n, int a, int b) {
    memmove(n->buf + a, n->buf + b, n->len - b + 1);
    n->len -= b - a;
    n->cur = a;
    n->anchor = -1;
}
static void delete_sel(notes_t *n) { if (has_sel(n)) delete_range(n, sel_a(n), sel_b(n)); n->anchor = -1; }
static void insert_text(notes_t *n, const char *t, int len) {
    int room = CAP - 1 - n->len;
    if (len > room) len = room;
    if (len <= 0) return;
    memmove(n->buf + n->cur + len, n->buf + n->cur, n->len - n->cur + 1);
    memcpy(n->buf + n->cur, t, len);
    n->cur += len;
    n->len += len;
}
static void insert(notes_t *n, char c) { insert_text(n, &c, 1); }

static int is_word(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

static void move_vert(notes_t *n, int lines) {
    int ls = line_start(n, n->cur);
    int px = text_width_n(n->buf + ls, n->cur - ls, &font_ui);
    for (int i = 0; i < ABS(lines); i++) {
        if (lines < 0) {
            if (ls == 0) { if (i == 0) { n->cur = 0; return; } break; }
            ls = line_start(n, ls - 1);
        } else {
            int le = line_end(n, ls);
            if (le >= n->len) { if (i == 0) { n->cur = n->len; return; } break; }
            ls = le + 1;
        }
    }
    int le = line_end(n, ls), c = ls;
    while (c < le && text_width_n(n->buf + ls, c - ls + 1, &font_ui) <= px + 3) c++;
    n->cur = c;
}

static void n_key(window_t *w, key_event_t *e) {
    notes_t *n = w->data;
    if (!n) return;
    int k = e->key, ctrl = e->mods & MOD_CTRL, shift = e->mods & MOD_SHIFT;
    n->blink_on = 1;
    n->blink_at = ticks_ms();
    n->follow = 1;
    if (ctrl && k >= 'A' && k <= 'Z') k += 32;
    if (ctrl) {
        if (k == 's') { save(w, n); }
        else if (k == 'a') { n->anchor = 0; n->cur = n->len; }
        else if (k == 'c' || k == 'x') {
            if (has_sel(n)) {
                clipboard_set(n->buf + sel_a(n), sel_b(n) - sel_a(n));
                if (k == 'x') { before_edit(n, EK_OTHER); delete_sel(n); }
            }
        } else if (k == 'v') {
            size_t len;
            const char *c = clipboard_get(&len);
            if (len) {
                before_edit(n, EK_OTHER);
                delete_sel(n);
                for (size_t i = 0; i < len; i++) if (c[i] != '\r') insert(n, c[i]);
            }
        } else if (k == 'z') { if (shift) redo(n); else undo(n); }
        else if (k == 'y') redo(n);
        else if (k == KEY_HOME) { if (shift && n->anchor < 0) n->anchor = n->cur; if (!shift) n->anchor = -1; n->cur = 0; }
        else if (k == KEY_END) { if (shift && n->anchor < 0) n->anchor = n->cur; if (!shift) n->anchor = -1; n->cur = n->len; }
        else if (k == KEY_LEFT || k == KEY_RIGHT) {
            if (shift && n->anchor < 0) n->anchor = n->cur;
            if (!shift) n->anchor = -1;
            if (k == KEY_LEFT) { while (n->cur > 0 && !is_word(n->buf[n->cur - 1])) n->cur--; while (n->cur > 0 && is_word(n->buf[n->cur - 1])) n->cur--; }
            else { while (n->cur < n->len && !is_word(n->buf[n->cur])) n->cur++; while (n->cur < n->len && is_word(n->buf[n->cur])) n->cur++; }
        } else if (k == KEY_BACKSPACE) {
            if (has_sel(n)) { before_edit(n, EK_OTHER); delete_sel(n); }
            else {
                int a = n->cur;
                while (a > 0 && !is_word(n->buf[a - 1]) && n->buf[a - 1] != '\n') a--;
                while (a > 0 && is_word(n->buf[a - 1])) a--;
                if (a == n->cur && a > 0) a--;
                if (a < n->cur) { before_edit(n, EK_OTHER); delete_range(n, a, n->cur); }
            }
        }
        retitle(w, n);
        wm_invalidate(w);
        return;
    }
    int nav = k == KEY_LEFT || k == KEY_RIGHT || k == KEY_UP || k == KEY_DOWN || k == KEY_HOME || k == KEY_END || k == KEY_PGUP || k == KEY_PGDN;
    if (nav) {
        if (shift) { if (n->anchor < 0) n->anchor = n->cur; }
        else if (has_sel(n) && (k == KEY_LEFT || k == KEY_RIGHT)) {
            /* collapse the selection to the side the arrow points to */
            n->cur = k == KEY_LEFT ? sel_a(n) : sel_b(n);
            n->anchor = -1;
            wm_invalidate(w);
            return;
        } else n->anchor = -1;
        n->last_kind = 0;
        switch (k) {
        case KEY_LEFT: if (n->cur) n->cur--; break;
        case KEY_RIGHT: if (n->cur < n->len) n->cur++; break;
        case KEY_HOME: {
            /* smart home: first non-space character, then column 0 */
            int ls = line_start(n, n->cur), fs = ls;
            while (fs < n->len && n->buf[fs] == ' ') fs++;
            n->cur = (n->cur == fs) ? ls : fs;
            break;
        }
        case KEY_END: n->cur = line_end(n, n->cur); break;
        case KEY_UP: move_vert(n, -1); break;
        case KEY_DOWN: move_vert(n, 1); break;
        case KEY_PGUP: move_vert(n, -15); break;
        case KEY_PGDN: move_vert(n, 15); break;
        }
    } else if (k == KEY_ENTER) {
        before_edit(n, EK_OTHER);
        delete_sel(n);
        int ls = line_start(n, n->cur), ind = 0;
        while (ls + ind < n->cur && n->buf[ls + ind] == ' ') ind++;
        insert(n, '\n');
        while (ind--) insert(n, ' ');
    } else if (k == KEY_TAB) {
        before_edit(n, EK_TYPE);
        delete_sel(n);
        insert_text(n, "    ", 4);
    } else if (k == KEY_BACKSPACE) {
        if (has_sel(n)) { before_edit(n, EK_OTHER); delete_sel(n); }
        else if (n->cur > 0) { before_edit(n, EK_DELETE); delete_range(n, n->cur - 1, n->cur); }
    } else if (k == KEY_DELETE) {
        if (has_sel(n)) { before_edit(n, EK_OTHER); delete_sel(n); }
        else if (n->cur < n->len) { before_edit(n, EK_DELETE); delete_range(n, n->cur, n->cur + 1); }
    } else if (k == KEY_ESC) {
        n->anchor = -1;
    } else if (k >= 32 && k < 127 && !(e->mods & MOD_ALT)) {
        before_edit(n, has_sel(n) ? EK_OTHER : EK_TYPE);
        delete_sel(n);
        insert(n, (char)k);
    }
    retitle(w, n);
    wm_invalidate(w);
}

static int pos_at(notes_t *n, int x, int y) {
    int line = n->top + (y - BAR - 10) / LH;
    if (y < BAR + 10) line = n->top - 1;
    if (line < 0) return 0;
    if (line > line_of(n, n->len)) return n->len;
    int p = pos_of_line(n, line);
    int e = line_end(n, p), c = p;
    int tx = x - PADX + n->left;
    while (c < e && text_width_n(n->buf + p, c - p, &font_ui) + font_ui.g[n->buf[c] > 32 && n->buf[c] < 127 ? n->buf[c] - 32 : 0].adv / 2 < tx) c++;
    return c;
}

static void n_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    notes_t *n = w->data;
    if (!n) return;
    if (ev == ME_DOWN && (b & MB_LEFT) && y >= BAR) {
        int p = pos_at(n, x, y);
        uint32_t now = ticks_ms();
        if (now - n->last_click < 400 && p == n->cur && !has_sel(n)) {
            /* double-click selects a word */
            int a = p, z = p;
            while (a > 0 && is_word(n->buf[a - 1])) a--;
            while (z < n->len && is_word(n->buf[z])) z++;
            n->anchor = a;
            n->cur = z;
            n->last_click = 0;
        } else {
            if (keyboard_mods() & MOD_SHIFT) { if (n->anchor < 0) n->anchor = n->cur; }
            else n->anchor = p;
            n->cur = p;
            n->dragging = 1;
            n->last_click = now;
        }
        n->blink_on = 1;
        n->blink_at = now;
        n->last_kind = 0;
        wm_invalidate(w);
    } else if (ev == ME_MOVE && n->dragging && (b & MB_LEFT)) {
        int p = pos_at(n, x, y);
        if (p != n->cur) { n->cur = p; n->follow = 1; wm_invalidate(w); }
    } else if (ev == ME_UP) {
        n->dragging = 0;
        if (n->anchor == n->cur) n->anchor = -1;
    } else if (ev == ME_WHEEL) {
        int total = line_of(n, n->len) + 1;
        n->top = CLAMP(n->top + wheel * 3, 0, MAX(0, total - 1));
        wm_invalidate(w);
    }
}

static void n_tick(window_t *w, uint32_t now) {
    notes_t *n = w->data;
    if (!n) return;
    int on = ((now - n->blink_at) / 530) % 2 == 0;
    if (on != n->blink_on) { n->blink_on = on; wm_invalidate(w); }
    if (n->saved && n->saved_flash && now - n->saved_flash > 1200) { n->saved_flash = 0; wm_invalidate(w); }
}

static void n_close(window_t *w) {
    notes_t *n = w->data;
    if (!n) return;
    if (!n->saved) fs_write(n->name, n->buf, n->len); /* autosave */
    while (n->nundo) snap_free(&n->undo[--n->nundo]);
    clear_redo(n);
}

const app_t app_notes = {"Notes", "Text editor", ICON_NOTES, 640, 460, 1, 0,
                         n_init, n_paint, n_key, n_mouse, n_tick, n_close};
