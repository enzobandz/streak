/* Notes: a plain text editor backed by the in-memory file system. */
#include "../kernel/wm.h"

#define CAP 32768
typedef struct {
    char name[FS_NAME_LEN];
    char buf[CAP];
    int len, cur, top, saved;
    uint32_t blink_at, saved_flash;
    int blink_on, sel_drag;
} notes_t;
#define BAR 40
#define PADX 16

static void load(notes_t *n, const char *name) {
    strlcpy(n->name, name, sizeof n->name);
    file_t *f = fs_find(name);
    n->len = f ? (int)MIN(f->size, CAP - 1) : 0;
    if (f) memcpy(n->buf, f->data, n->len);
    n->buf[n->len] = 0;
    n->cur = 0;
    n->top = 0;
    n->saved = 1;
}
static void save(window_t *w, notes_t *n) {
    fs_write(n->name, n->buf, n->len);
    n->saved = 1;
    n->saved_flash = ticks_ms();
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
    load(n, "notes.txt");
    retitle(w, n);
}

void wm_open_file_in_notes(const char *name) {
    window_t *w = wm_open(&app_notes);
    if (!w) return;
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

static void n_paint(window_t *w, surface_t *s) {
    notes_t *n = w->data;
    theme_t *t = &g_theme;
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->dark ? RGB(0x1A, 0x1A, 0x1E) : RGB(0xFF, 0xFF, 0xFF));
    /* toolbar */
    fill_rect(s, mkrect(0, 0, s->w, BAR), t->title_bg);
    fill_rect(s, mkrect(0, BAR - 1, s->w, 1), t->border);
    draw_text(s, PADX, (BAR - font_small.height) / 2, n->name, &font_small, t->text_dim);
    const char *st = n->saved ? "Saved" : "Ctrl+S to save";
    uint32_t sc = t->text_dim;
    if (n->saved && ticks_ms() - n->saved_flash < 1200) sc = t->accent;
    draw_text(s, s->w - text_width(st, &font_small) - PADX, (BAR - font_small.height) / 2, st, &font_small, sc);
    /* text */
    int lh = font_ui.height + 4;
    int rows = (s->h - BAR - 12) / lh;
    int cl = line_of(n, n->cur);
    if (cl < n->top) n->top = cl;
    if (cl >= n->top + rows) n->top = cl - rows + 1;
    int p = pos_of_line(n, n->top);
    int y = BAR + 10;
    int ln = n->top;
    while (y + lh <= s->h && p <= n->len) {
        int e = line_end(n, p);
        draw_text_n(s, PADX, y, n->buf + p, e - p, &font_ui, t->text);
        if (ln == cl && n->blink_on && w == wm_focused()) {
            int cx = PADX + text_width_n(n->buf + p, n->cur - p, &font_ui);
            fill_rect(s, mkrect(cx, y, 2, font_ui.height), t->accent);
        }
        if (e >= n->len) break;
        p = e + 1;
        y += lh;
        ln++;
    }
}

static void insert(notes_t *n, char c) {
    if (n->len >= CAP - 1) return;
    memmove(n->buf + n->cur + 1, n->buf + n->cur, n->len - n->cur + 1);
    n->buf[n->cur++] = c;
    n->len++;
    n->saved = 0;
}

static void n_key(window_t *w, key_event_t *e) {
    notes_t *n = w->data;
    int k = e->key;
    n->blink_on = 1;
    n->blink_at = ticks_ms();
    if ((e->mods & MOD_CTRL) && (k == 's' || k == 'S')) { save(w, n); retitle(w, n); return; }
    if (k == KEY_ENTER) {
        /* keep indentation */
        int ls = line_start(n, n->cur), ind = 0;
        while (ls + ind < n->cur && n->buf[ls + ind] == ' ') ind++;
        insert(n, '\n');
        while (ind--) insert(n, ' ');
    } else if (k == KEY_TAB) { insert(n, ' '); insert(n, ' '); insert(n, ' '); insert(n, ' '); }
    else if (k == KEY_BACKSPACE) {
        if (n->cur > 0) { memmove(n->buf + n->cur - 1, n->buf + n->cur, n->len - n->cur + 1); n->cur--; n->len--; n->saved = 0; }
    } else if (k == KEY_DELETE) {
        if (n->cur < n->len) { memmove(n->buf + n->cur, n->buf + n->cur + 1, n->len - n->cur); n->len--; n->saved = 0; }
    } else if (k == KEY_LEFT) { if (n->cur) n->cur--; }
    else if (k == KEY_RIGHT) { if (n->cur < n->len) n->cur++; }
    else if (k == KEY_HOME) n->cur = line_start(n, n->cur);
    else if (k == KEY_END) n->cur = line_end(n, n->cur);
    else if (k == KEY_UP || k == KEY_DOWN || k == KEY_PGUP || k == KEY_PGDN) {
        int ls = line_start(n, n->cur);
        int px = text_width_n(n->buf + ls, n->cur - ls, &font_ui);
        int steps = (k == KEY_PGUP || k == KEY_PGDN) ? 15 : 1;
        int up = k == KEY_UP || k == KEY_PGUP;
        for (int i = 0; i < steps; i++) {
            if (up) { if (ls == 0) { n->cur = 0; break; } ls = line_start(n, ls - 1); }
            else { int le = line_end(n, ls); if (le >= n->len) { break; } ls = le + 1; }
        }
        int le = line_end(n, ls), c = ls;
        while (c < le && text_width_n(n->buf + ls, c - ls + 1, &font_ui) <= px + 3) c++;
        n->cur = c;
    } else if (k >= 32 && k < 127 && !(e->mods & MOD_CTRL)) insert(n, (char)k);
    retitle(w, n);
    wm_invalidate(w);
}

static void place_cursor(window_t *w, notes_t *n, int x, int y) {
    int lh = font_ui.height + 4;
    if (y < BAR) return;
    int line = n->top + (y - BAR - 10) / lh;
    if (line < 0) line = 0;
    int p = pos_of_line(n, line);
    if (line > line_of(n, n->len)) { n->cur = n->len; wm_invalidate(w); return; }
    int e = line_end(n, p), c = p;
    while (c < e && PADX + text_width_n(n->buf + p, c - p, &font_ui) + font_ui.g['n' - 32].adv / 2 < x) c++;
    n->cur = c;
    n->blink_on = 1;
    n->blink_at = ticks_ms();
    wm_invalidate(w);
}

static void n_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    notes_t *n = w->data;
    if (ev == ME_DOWN && (b & MB_LEFT)) place_cursor(w, n, x, y);
    if (ev == ME_WHEEL) {
        n->top = MAX(0, n->top - wheel * 3);
        /* keep the caret on screen by moving it into view */
        int cl = line_of(n, n->cur);
        if (cl < n->top) n->cur = pos_of_line(n, n->top);
        wm_invalidate(w);
    }
}

static void n_tick(window_t *w, uint32_t now) {
    notes_t *n = w->data;
    int on = ((now - n->blink_at) / 530) % 2 == 0;
    if (on != n->blink_on) { n->blink_on = on; wm_invalidate(w); }
    if (n->saved && n->saved_flash && now - n->saved_flash > 1200) { n->saved_flash = 0; wm_invalidate(w); }
}

static void n_close(window_t *w) {
    notes_t *n = w->data;
    if (!n->saved) fs_write(n->name, n->buf, n->len); /* autosave */
}

const app_t app_notes = {"Notes", "Text editor", ICON_NOTES, 640, 460, 1, 0,
                         n_init, n_paint, n_key, n_mouse, n_tick, n_close};
