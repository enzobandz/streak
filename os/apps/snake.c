/* Snake, with smoothly interpolated movement between grid cells. */
#include "../kernel/wm.h"

#define GW 24
#define GH 18
#define CELL 22
#define HUD 40
typedef struct {
    int x[GW * GH], y[GW * GH], px[GW * GH], py[GW * GH];
    int len, dir, ndir, fx, fy, score, best, over, paused;
    uint32_t last_step, step_ms;
} snake_t;

static void place_food(snake_t *s) {
    for (;;) {
        s->fx = rand32() % GW;
        s->fy = rand32() % GH;
        int ok = 1;
        for (int i = 0; i < s->len; i++) if (s->x[i] == s->fx && s->y[i] == s->fy) ok = 0;
        if (ok) return;
    }
}
static void reset(snake_t *s) {
    s->len = 4;
    for (int i = 0; i < s->len; i++) { s->x[i] = s->px[i] = 8 - i; s->y[i] = s->py[i] = GH / 2; }
    s->dir = s->ndir = 0;
    s->score = 0;
    s->over = 0;
    s->step_ms = 120;
    s->last_step = ticks_ms();
    place_food(s);
}
static void k_init(window_t *w) { snake_t *s = kzalloc(sizeof *s); w->data = s; reset(s); s->paused = 1; }

static void k_paint(window_t *w, surface_t *sf) {
    snake_t *s = w->data;
    theme_t *t = &g_theme;
    uint32_t now = ticks_ms();
    fill_rect(sf, mkrect(0, 0, sf->w, sf->h), t->dark ? RGB(0x15, 0x17, 0x1C) : RGB(0xF4, 0xF6, 0xF2));
    char b[64];
    ksnprintf(b, sizeof b, "Score %d", s->score);
    draw_text(sf, 16, 10, b, &font_bold, t->text);
    ksnprintf(b, sizeof b, "Best %d", s->best);
    draw_text(sf, sf->w - 16 - text_width(b, &font_ui), 10, b, &font_ui, t->text_dim);
    int ox = (sf->w - GW * CELL) / 2, oy = HUD;
    fill_round_rect(sf, mkrect(ox, oy, GW * CELL, GH * CELL), 10, t->dark ? RGB(0x1D, 0x20, 0x27) : RGB(0xE8, 0xEE, 0xE4));
    float k = s->paused || s->over ? 1 : (float)(now - s->last_step) / s->step_ms;
    if (k > 1) k = 1;
    /* food pulses */
    float pulse = (float)((now / 8) % 100) / 100;
    pulse = pulse < 0.5f ? pulse * 2 : (1 - pulse) * 2;
    fill_circle(sf, ox + s->fx * CELL + CELL / 2, oy + s->fy * CELL + CELL / 2, 6 + (int)(pulse * 2), RGB(0xFF, 0x4D, 0x6D));
    for (int i = s->len - 1; i >= 0; i--) {
        float x = s->px[i] + (s->x[i] - s->px[i]) * k, y = s->py[i] + (s->y[i] - s->py[i]) * k;
        int cx = ox + (int)(x * CELL) + CELL / 2, cy = oy + (int)(y * CELL) + CELL / 2;
        uint32_t c = mix(RGB(0x5E, 0xE0, 0x7A), RGB(0x1F, 0x9D, 0x5A), i * 255 / MAX(s->len, 1));
        fill_circle(sf, cx, cy, i == 0 ? 10 : 9, c);
    }
    if (s->paused || s->over) {
        rect_t r = mkrect(sf->w / 2 - 150, oy + GH * CELL / 2 - 46, 300, 92);
        fill_round_rect(sf, r, 12, t->dark ? 0xE0202024 : 0xE8FFFFFF);
        draw_text_centered(sf, mkrect(r.x, r.y + 12, r.w, 30), s->over ? "Game over" : "Snake", &font_title, t->text);
        draw_text_centered(sf, mkrect(r.x, r.y + 52, r.w, 24), "Press Space to play - arrows to steer", &font_small, t->text_dim);
    }
}

static void step(snake_t *s) {
    s->dir = s->ndir;
    static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    int nx = s->x[0] + dx[s->dir], ny = s->y[0] + dy[s->dir];
    if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) { s->over = 1; return; }
    for (int i = 0; i < s->len - 1; i++) if (s->x[i] == nx && s->y[i] == ny) { s->over = 1; return; }
    int grow = nx == s->fx && ny == s->fy;
    int oldlen = s->len;
    if (grow && s->len < GW * GH) s->len++;
    /* each segment animates from where it was to where it is now */
    for (int i = 0; i < oldlen; i++) { s->px[i] = s->x[i]; s->py[i] = s->y[i]; }
    for (int i = s->len - 1; i > 0; i--) { s->x[i] = s->x[i - 1]; s->y[i] = s->y[i - 1]; }
    s->x[0] = nx; s->y[0] = ny;
    if (s->len > oldlen) { s->px[oldlen] = s->x[oldlen]; s->py[oldlen] = s->y[oldlen]; }
    if (grow) {
        s->score += 10;
        if (s->score > s->best) s->best = s->score;
        if (s->step_ms > 70) s->step_ms -= 3;
        place_food(s);
    }
}

static void k_key(window_t *w, key_event_t *e) {
    snake_t *s = w->data;
    int k = e->key;
    if (k == ' ' || k == KEY_ENTER) {
        if (s->over) reset(s);
        s->paused = !s->paused || s->over;
        s->last_step = ticks_ms();
    }
    int nd = -1;
    if (k == KEY_RIGHT || k == 'd') nd = 0;
    if (k == KEY_DOWN || k == 's') nd = 1;
    if (k == KEY_LEFT || k == 'a') nd = 2;
    if (k == KEY_UP || k == 'w') nd = 3;
    if (nd >= 0 && (nd + 2) % 4 != s->dir) {
        s->ndir = nd;
        if (s->paused && !s->over) { s->paused = 0; s->last_step = ticks_ms(); }
    }
    wm_invalidate(w);
}

static void k_tick(window_t *w, uint32_t now) {
    snake_t *s = w->data;
    if (!s->paused && !s->over && now - s->last_step >= s->step_ms) {
        s->last_step = now;
        step(s);
    }
    wm_invalidate(w); /* animate every frame */
}

const app_t app_snake = {"Snake", "Arcade game", ICON_SNAKE, GW * CELL + 32, GH * CELL + HUD + 16, 0, 1,
                         k_init, k_paint, k_key, 0, k_tick, 0};
