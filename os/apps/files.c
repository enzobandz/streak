/* Files: browse the in-memory file system. */
#include "../kernel/wm.h"

typedef struct { int sel, hover; uint32_t last_click; int last_idx, counter; } files_t;
#define HDR 52
#define ROW 44

static void f_init(window_t *w) { files_t *f = kzalloc(sizeof *f); w->data = f; if (!f) return; f->sel = 0; f->hover = -1; f->last_idx = -1; }
static rect_t new_btn(void) { return mkrect(16, 10, 110, 32); }
static rect_t del_btn(void) { return mkrect(134, 10, 90, 32); }
static int count(void) { int n = 0; while (fs_list_visible(n)) n++; return n; }

static void f_paint(window_t *w, surface_t *s) {
    files_t *f = w->data;
    theme_t *t = &g_theme;
    fill_rect(s, mkrect(0, 0, s->w, s->h), t->win_bg);
    fill_rect(s, mkrect(0, 0, s->w, HDR), t->title_bg);
    fill_rect(s, mkrect(0, HDR - 1, s->w, 1), t->border);
    ui_button(s, new_btn(), "New file", f->hover == 1000, 0, 1);
    ui_button(s, del_btn(), "Delete", f->hover == 1001, 0, 0);
    draw_text(s, s->w - 16 - text_width("ram:/", &font_small), (HDR - font_small.height) / 2, "ram:/", &font_small, t->text_dim);
    int n = count();
    draw_text(s, 64, HDR + 8, "Name", &font_small, t->text_dim);
    draw_text(s, s->w - 120, HDR + 8, "Size", &font_small, t->text_dim);
    for (int i = 0; i < n; i++) {
        file_t *fl = fs_list_visible(i);
        rect_t r = mkrect(8, HDR + 30 + i * ROW, s->w - 16, ROW - 4);
        if (r.y > s->h) break;
        if (i == f->sel) fill_round_rect(s, r, 8, WITH_ALPHA(t->accent, 0x38));
        else if (i == f->hover) fill_round_rect(s, r, 8, t->hover);
        draw_app_icon(s, r.x + 12, r.y + 6, 28, ICON_NOTES);
        draw_text(s, 64, r.y + (r.h - font_ui.height) / 2, fl->name, &font_ui, t->text);
        char b[24];
        ksnprintf(b, sizeof b, "%u B", (unsigned)fl->size);
        draw_text(s, s->w - 120, r.y + (r.h - font_ui.height) / 2, b, &font_ui, t->text_dim);
    }
    if (!n) draw_text_centered(s, mkrect(0, HDR, s->w, 120), "No files yet - create one with New file.", &font_ui, t->text_dim);
    draw_text(s, 16, s->h - 26, "Double-click to open in Notes. Files live in memory until shutdown.", &font_small, t->text_dim);
}

static int hit(window_t *w, int x, int y) {
    (void)w;
    if (rect_contains(new_btn(), x, y)) return 1000;
    if (rect_contains(del_btn(), x, y)) return 1001;
    if (y < HDR + 30) return -1;
    int i = (y - HDR - 30) / ROW;
    return i < count() ? i : -1;
}

static void f_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    (void)b; (void)wheel;
    files_t *f = w->data;
    int h = hit(w, x, y);
    if (h != f->hover) { f->hover = h; wm_invalidate(w); }
    if (ev != ME_DOWN) return;
    if (h == 1000) {
        char name[32];
        do ksnprintf(name, sizeof name, "untitled-%d.txt", ++f->counter); while (fs_find(name));
        fs_write(name, "", 0);
        wm_open_file_in_notes(name);
    } else if (h == 1001) {
        file_t *fl = fs_list_visible(f->sel);
        if (fl) fs_remove(fl->name);
        if (f->sel >= count()) f->sel = MAX(0, count() - 1);
    } else if (h >= 0) {
        uint32_t now = ticks_ms();
        if (h == f->last_idx && now - f->last_click < 450) { file_t *fl = fs_list_visible(h); if (fl) wm_open_file_in_notes(fl->name); }
        f->sel = h;
        f->last_idx = h;
        f->last_click = now;
    }
    wm_invalidate(w);
}

static void f_key(window_t *w, key_event_t *e) {
    files_t *f = w->data;
    int n = count();
    if (e->key == KEY_DOWN && f->sel < n - 1) f->sel++;
    else if (e->key == KEY_UP && f->sel > 0) f->sel--;
    else if (e->key == KEY_ENTER) { file_t *fl = fs_list_visible(f->sel); if (fl) wm_open_file_in_notes(fl->name); }
    else if (e->key == KEY_DELETE) { file_t *fl = fs_list_visible(f->sel); if (fl) fs_remove(fl->name); if (f->sel >= count()) f->sel = MAX(0, count() - 1); }
    wm_invalidate(w);
}

static void f_tick(window_t *w, uint32_t now) {
    /* refresh occasionally so files written elsewhere appear */
    static uint32_t last;
    if (now - last > 500) { last = now; wm_invalidate(w); }
}

const app_t app_files = {"Files", "Browse files", ICON_FILES, 520, 420, 1, 0,
                         f_init, f_paint, f_key, f_mouse, f_tick, 0};
