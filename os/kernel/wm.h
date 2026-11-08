/* Fluvia window manager / desktop shell API used by apps. */
#pragma once
#include "kernel.h"

#define TITLE_H 36
#define TASKBAR_H 52
#define WIN_RADIUS 9

enum { ME_DOWN, ME_UP, ME_MOVE, ME_WHEEL, ME_LEAVE };

typedef struct window window_t;
typedef struct app {
    const char *name;
    const char *desc;
    int icon;
    int w, h;                 /* default content size */
    int resizable;
    int single;               /* only one instance */
    void (*init)(window_t *w);
    void (*paint)(window_t *w, surface_t *s);
    void (*key)(window_t *w, key_event_t *e);
    void (*mouse)(window_t *w, int x, int y, int buttons, int ev, int wheel);
    void (*tick)(window_t *w, uint32_t now);
    void (*close)(window_t *w);
} app_t;

enum { WS_NORMAL, WS_OPENING, WS_CLOSING, WS_MINIMIZING, WS_RESTORING, WS_MORPH };

struct window {
    int id;
    const app_t *app;
    char title[64];
    int x, y, w, h;           /* frame rectangle on screen */
    surface_t *surf;
    int dirty;
    int minimized, maximized, snapped;
    rect_t restore;
    int state;
    anim_t anim;
    rect_t from, to;
    float a_from, a_to;
    rect_t last_drawn;
    int hover_btn, press_btn;
    void *data;
};

typedef struct {
    uint32_t desktop, win_bg, title_bg, text, text_dim, border, hover, press, field, accent, accent_text;
    uint32_t taskbar_tint, menu_tint;
    int dark;
} theme_t;
extern theme_t g_theme;
extern int g_accent_idx, g_wallpaper_idx;
extern const uint32_t g_accents[6];

/* apps */
enum { ICON_TERMINAL, ICON_NOTES, ICON_PAINT, ICON_CALC, ICON_MONITOR, ICON_SETTINGS, ICON_SNAKE, ICON_FILES, ICON_COUNT };
extern const app_t app_terminal, app_notes, app_paint, app_calc, app_monitor, app_settings, app_snake, app_files;
extern const app_t *g_apps[];
extern int g_app_count;

window_t *wm_open(const app_t *app);
void wm_close(window_t *w);
void wm_invalidate(window_t *w);
void wm_set_title(window_t *w, const char *t);
void wm_focus(window_t *w);
window_t *wm_focused(void);
int wm_window_count(void);
window_t *wm_window_at(int idx);
rect_t wm_content_rect(window_t *w);
void wm_damage(rect_t r);
void wm_damage_all(void);
void wm_init(void);
void wm_frame(uint32_t now);
void wm_apply_theme(void);
void wm_open_file_in_notes(const char *name);
void wm_shutdown(int restart);
void wm_taskbar_click(window_t *w);

/* shell */
void shell_init(void);
void shell_generate_wallpaper(int idx);
void draw_app_icon(surface_t *s, int x, int y, int size, int icon);
void toast(const char *title, const char *body);
void theme_set(int dark, int accent_idx);
void settings_save(void);

/* widgets shared by apps */
void ui_button(surface_t *s, rect_t r, const char *label, int hover, int pressed, int primary);

/* shell <-> wm internals */
void shell_compose(void);                    /* wallpaper is done by wm; this draws taskbar, start, toasts */
void shell_compose_wallpaper(rect_t r);
int shell_mouse(int x, int y, int buttons, int ev, int wheel); /* 1 = consumed */
int shell_key(key_event_t *e);               /* 1 = consumed */
void shell_tick(uint32_t now);
rect_t shell_icon_rect_for(const app_t *app);
void shell_toggle_start(void);
int shell_start_open(void);
void shell_windows_changed(void);
