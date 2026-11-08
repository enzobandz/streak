/* Terminal: a small shell over the in-memory file system and window manager. */
#include "../kernel/wm.h"

#define TL 400
#define TC 120
typedef struct {
    char lines[TL][TC];
    uint8_t kind[TL];
    int n, scroll;
    char in[TC];
    int len, cur;
    char hist[16][TC];
    int nhist, hpos;
    uint32_t blink_at;
    int blink_on;
} term_t;
enum { K_OUT, K_CMD, K_ERR, K_ACC, K_DIM };
static const char *PROMPT = "guest@streak ~ $ ";

static void tputk(term_t *t, int kind, const char *s) {
    /* split on newlines and wrap long lines */
    while (1) {
        const char *nl = strchr(s, '\n');
        int len = nl ? (int)(nl - s) : (int)strlen(s);
        do {
            int take = MIN(len, TC - 1);
            if (t->n == TL) {
                memmove(t->lines[0], t->lines[1], sizeof(t->lines[0]) * (TL - 1));
                memmove(t->kind, t->kind + 1, TL - 1);
                t->n--;
            }
            memcpy(t->lines[t->n], s, take);
            t->lines[t->n][take] = 0;
            t->kind[t->n++] = kind;
            s += take;
            len -= take;
        } while (len > 0);
        if (!nl) break;
        s = nl + 1;
    }
    t->scroll = 0;
}
static void tprintf(term_t *t, int kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    tputk(t, kind, buf);
}

/* tiny recursive-descent calculator for `calc` */
static const char *ep;
static double expr(void);
static void skip(void) { while (*ep == ' ') ep++; }
static double atom(void) {
    skip();
    if (*ep == '(') { ep++; double v = expr(); skip(); if (*ep == ')') ep++; return v; }
    if (*ep == '-') { ep++; return -atom(); }
    double v = 0, sc = 1;
    int dot = 0;
    while ((*ep >= '0' && *ep <= '9') || *ep == '.') {
        if (*ep == '.') dot = 1;
        else if (dot) { sc /= 10; v += (*ep - '0') * sc; }
        else v = v * 10 + (*ep - '0');
        ep++;
    }
    return v;
}
static double term(void) {
    double v = atom();
    for (;;) {
        skip();
        if (*ep == '*' || *ep == 'x') { ep++; v *= atom(); }
        else if (*ep == '/') { ep++; double d = atom(); v = d == 0 ? 0.0 / 0.0 : v / d; }
        else if (*ep == '%') { ep++; double d = atom(); v = d == 0 ? 0.0 / 0.0 : v - d * (long long)(v / d); }
        else return v;
    }
}
static double expr(void) {
    double v = term();
    for (;;) {
        skip();
        if (*ep == '+') { ep++; v += term(); }
        else if (*ep == '-') { ep++; v -= term(); }
        else return v;
    }
}

static const app_t *find_app(const char *name) {
    for (int i = 0; i < g_app_count; i++)
        if (str_startswith_ci(g_apps[i]->name, name)) return g_apps[i];
    return 0;
}

static void run(window_t *w, term_t *t, char *cmd) {
    while (*cmd == ' ') cmd++;
    char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }
    if (!*cmd) return;
    if (!strcmp(cmd, "help")) {
        tputk(t, K_ACC, "Commands");
        tputk(t, K_OUT, "  help              this list            clear            clear the screen\n"
                        "  ls                list files           cat <file>       print a file\n"
                        "  write <f> <text>  create a file        rm <file>        delete a file\n"
                        "  edit <file>       open in Notes        open <app>       launch an app\n"
                        "  ps                list windows         kill <id>        close a window\n"
                        "  calc <expr>       e.g. calc (2+3)*7    echo <text>      print text\n"
                        "  date / uptime     time                 mem / sysinfo    system info\n"
                        "  theme dark|light  switch theme         wallpaper <0-3>  change wallpaper\n"
                        "  neofetch          system summary       reboot / shutdown");
    } else if (!strcmp(cmd, "clear") || !strcmp(cmd, "cls")) {
        t->n = 0;
    } else if (!strcmp(cmd, "echo")) {
        tputk(t, K_OUT, arg);
    } else if (!strcmp(cmd, "ls") || !strcmp(cmd, "dir")) {
        file_t *f;
        for (int i = 0; (f = fs_list(i)); i++) tprintf(t, K_OUT, "  %-24s %6u bytes", f->name, (unsigned)f->size);
    } else if (!strcmp(cmd, "cat") || !strcmp(cmd, "type")) {
        file_t *f = fs_find(arg);
        if (f) tputk(t, K_OUT, f->data); else tprintf(t, K_ERR, "cat: %s: no such file", arg);
    } else if (!strcmp(cmd, "write")) {
        char *text = arg;
        while (*text && *text != ' ') text++;
        if (*text) *text++ = 0;
        if (!*arg) tputk(t, K_ERR, "usage: write <file> <text>");
        else if (fs_write(arg, text, strlen(text))) tprintf(t, K_DIM, "wrote %s", arg);
        else tputk(t, K_ERR, "write: file system full");
    } else if (!strcmp(cmd, "rm") || !strcmp(cmd, "del")) {
        if (!fs_remove(arg)) tprintf(t, K_ERR, "rm: %s: no such file", arg);
    } else if (!strcmp(cmd, "edit")) {
        wm_open_file_in_notes(*arg ? arg : "notes.txt");
    } else if (!strcmp(cmd, "open") || !strcmp(cmd, "start")) {
        const app_t *a = *arg ? find_app(arg) : 0;
        if (a) wm_open(a);
        else {
            tprintf(t, K_ERR, "open: unknown app '%s'. Apps:", arg);
            for (int i = 0; i < g_app_count; i++) tprintf(t, K_OUT, "  %s", g_apps[i]->name);
        }
    } else if (!strcmp(cmd, "ps")) {
        tputk(t, K_DIM, "  ID  APP             TITLE");
        for (int i = 0; i < wm_window_count(); i++) {
            window_t *x = wm_window_at(i);
            tprintf(t, K_OUT, "  %2d  %-15s %s%s", x->id, x->app->name, x->title, x->minimized ? " (minimised)" : "");
        }
    } else if (!strcmp(cmd, "kill")) {
        int id = 0;
        for (char *p = arg; *p >= '0' && *p <= '9'; p++) id = id * 10 + (*p - '0');
        for (int i = 0; i < wm_window_count(); i++)
            if (wm_window_at(i)->id == id) { wm_close(wm_window_at(i)); return; }
        tprintf(t, K_ERR, "kill: no window with id %s", arg);
    } else if (!strcmp(cmd, "calc")) {
        ep = arg;
        double v = expr();
        char b[64];
        fmt_double(b, sizeof b, v);
        tprintf(t, K_ACC, "= %s", b);
    } else if (!strcmp(cmd, "date") || !strcmp(cmd, "time")) {
        rtc_time_t r;
        rtc_read(&r);
        static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        tprintf(t, K_OUT, "%s %d-%02d-%02d %02d:%02d:%02d (RTC)", days[r.wday % 7], r.year, r.mon, r.day, r.hour, r.min, r.sec);
    } else if (!strcmp(cmd, "uptime")) {
        uint32_t s = ticks_ms() / 1000;
        tprintf(t, K_OUT, "up %u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
    } else if (!strcmp(cmd, "mem") || !strcmp(cmd, "free")) {
        tprintf(t, K_OUT, "heap: %u KiB used of %u KiB", (unsigned)(heap_used() / 1024), (unsigned)(heap_total() / 1024));
    } else if (!strcmp(cmd, "sysinfo") || !strcmp(cmd, "neofetch") || !strcmp(cmd, "uname")) {
        char cpu[48];
        cpu_name(cpu);
        if (!strcmp(cmd, "uname")) { tputk(t, K_OUT, "StreakOS 1.0 i686"); return; }
        uint32_t s = ticks_ms() / 1000;
        tprintf(t, K_ACC, "   /\\      guest@streak");
        tprintf(t, K_ACC, "  /  \\     ------------");
        tprintf(t, K_ACC, " / /\\ \\    OS:      StreakOS 1.0 (i686)");
        tprintf(t, K_ACC, "/_/  \\_\\   CPU:     %s", cpu);
        tprintf(t, K_OUT, "           Display: %dx%d, 32-bit colour", SCR_W, SCR_H);
        tprintf(t, K_OUT, "           Memory:  %u / %u MiB", (unsigned)(heap_used() >> 20), (unsigned)(heap_total() >> 20));
        tprintf(t, K_OUT, "           Uptime:  %u min %u s", s / 60, s % 60);
        tprintf(t, K_OUT, "           Pointer: %s", mouse_is_absolute() ? "absolute (VM integration)" : "PS/2 relative");
        tprintf(t, K_OUT, "           FPS:     %u", g_fps);
    } else if (!strcmp(cmd, "theme")) {
        if (!strcmp(arg, "light")) theme_set(0, g_accent_idx);
        else if (!strcmp(arg, "dark")) theme_set(1, g_accent_idx);
        else { tputk(t, K_ERR, "usage: theme dark|light"); return; }
        wm_apply_theme();
    } else if (!strcmp(cmd, "wallpaper")) {
        if (*arg >= '0' && *arg <= '3') shell_generate_wallpaper(*arg - '0');
        else tputk(t, K_ERR, "usage: wallpaper 0|1|2|3");
    } else if (!strcmp(cmd, "reboot")) {
        reboot();
    } else if (!strcmp(cmd, "shutdown") || !strcmp(cmd, "poweroff")) {
        power_off();
    } else if (!strcmp(cmd, "exit")) {
        wm_close(w);
    } else {
        tprintf(t, K_ERR, "%s: command not found (try 'help')", cmd);
    }
}

static void t_init(window_t *w) {
    term_t *t = kzalloc(sizeof *t);
    w->data = t;
    tputk(t, K_ACC, "StreakOS Terminal");
    tputk(t, K_DIM, "Type 'help' for a list of commands.");
    tputk(t, K_OUT, "");
}

static void t_paint(window_t *w, surface_t *s) {
    term_t *t = w->data;
    theme_t *th = &g_theme;
    uint32_t bg = th->dark ? RGB(0x12, 0x12, 0x16) : RGB(0xFF, 0xFF, 0xFF);
    fill_rect(s, mkrect(0, 0, s->w, s->h), bg);
    int lh = font_mono.height + 2;
    int rows = (s->h - 16) / lh;
    int total = t->n + 1;
    int first = MAX(0, total - rows - t->scroll);
    int y = 8;
    uint32_t col[5] = {th->text, th->text, th->dark ? RGB(0xFF, 0x6B, 0x6B) : RGB(0xC4, 0x2B, 0x1C), th->accent, th->text_dim};
    for (int i = first; i < total && y + lh <= s->h; i++, y += lh) {
        if (i < t->n) {
            if (t->kind[i] == K_CMD) {
                int x = draw_text(s, 12, y, PROMPT, &font_mono, th->accent);
                draw_text(s, x, y, t->lines[i] + strlen(PROMPT), &font_mono, th->text);
            } else draw_text(s, 12, y, t->lines[i], &font_mono, col[t->kind[i]]);
        } else {
            int x = draw_text(s, 12, y, PROMPT, &font_mono, th->accent);
            draw_text(s, x, y, t->in, &font_mono, th->text);
            if (t->blink_on && w == wm_focused()) {
                int cx = x + text_width_n(t->in, t->cur, &font_mono);
                fill_rect(s, mkrect(cx, y + 1, 2, font_mono.height - 2), th->accent);
            }
        }
    }
}

static void t_key(window_t *w, key_event_t *e) {
    term_t *t = w->data;
    int k = e->key;
    t->blink_on = 1;
    t->blink_at = ticks_ms();
    if ((e->mods & MOD_CTRL) && (k == 'l' || k == 'L')) { t->n = 0; }
    else if ((e->mods & MOD_CTRL) && (k == 'c' || k == 'C')) { char b[TC + 20]; ksnprintf(b, sizeof b, "%s%s^C", PROMPT, t->in); tputk(t, K_CMD, b); t->len = t->cur = 0; t->in[0] = 0; }
    else if (k == KEY_ENTER) {
        char line[TC + 20];
        ksnprintf(line, sizeof line, "%s%s", PROMPT, t->in);
        tputk(t, K_CMD, line);
        if (t->len) {
            if (t->nhist == 16) { memmove(t->hist[0], t->hist[1], sizeof(t->hist[0]) * 15); t->nhist--; }
            strcpy(t->hist[t->nhist++], t->in);
        }
        t->hpos = t->nhist;
        char cmd[TC];
        strcpy(cmd, t->in);
        t->len = t->cur = 0;
        t->in[0] = 0;
        run(w, t, cmd);
    } else if (k == KEY_BACKSPACE) {
        if (t->cur > 0) { memmove(t->in + t->cur - 1, t->in + t->cur, t->len - t->cur + 1); t->cur--; t->len--; }
    } else if (k == KEY_DELETE) {
        if (t->cur < t->len) { memmove(t->in + t->cur, t->in + t->cur + 1, t->len - t->cur); t->len--; }
    } else if (k == KEY_LEFT) { if (t->cur) t->cur--; }
    else if (k == KEY_RIGHT) { if (t->cur < t->len) t->cur++; }
    else if (k == KEY_HOME) t->cur = 0;
    else if (k == KEY_END) t->cur = t->len;
    else if (k == KEY_PGUP) t->scroll = MIN(t->scroll + 10, t->n);
    else if (k == KEY_PGDN) t->scroll = MAX(t->scroll - 10, 0);
    else if (k == KEY_UP || k == KEY_DOWN) {
        if (k == KEY_UP && t->hpos > 0) t->hpos--;
        else if (k == KEY_DOWN && t->hpos < t->nhist) t->hpos++;
        else return;
        strcpy(t->in, t->hpos < t->nhist ? t->hist[t->hpos] : "");
        t->len = t->cur = strlen(t->in);
    } else if (k == KEY_TAB) {
        /* complete app names after `open` and file names after cat/edit/rm */
        const char *sp = strchr(t->in, ' ');
        if (sp) {
            const char *frag = sp + 1;
            const char *match = 0;
            if (!strncmp(t->in, "open ", 5)) { for (int i = 0; i < g_app_count; i++) if (str_startswith_ci(g_apps[i]->name, frag)) { match = g_apps[i]->name; break; } }
            else { file_t *f; for (int i = 0; (f = fs_list(i)); i++) if (str_startswith_ci(f->name, frag)) { match = f->name; break; } }
            if (match && (int)(frag - t->in) + (int)strlen(match) < TC - 1) {
                strcpy(t->in + (frag - t->in), match);
                t->len = t->cur = strlen(t->in);
            }
        }
    } else if (k >= 32 && k < 127 && t->len < TC - 2) {
        memmove(t->in + t->cur + 1, t->in + t->cur, t->len - t->cur + 1);
        t->in[t->cur++] = (char)k;
        t->len++;
    }
    wm_invalidate(w);
}

static void t_mouse(window_t *w, int x, int y, int b, int ev, int wheel) {
    (void)x; (void)y; (void)b;
    term_t *t = w->data;
    if (ev == ME_WHEEL) {
        t->scroll = CLAMP(t->scroll - wheel * 3, 0, t->n);
        wm_invalidate(w);
    }
}

static void t_tick(window_t *w, uint32_t now) {
    term_t *t = w->data;
    int on = ((now - t->blink_at) / 530) % 2 == 0;
    if (on != t->blink_on) { t->blink_on = on; wm_invalidate(w); }
}

const app_t app_terminal = {"Terminal", "Command line shell", ICON_TERMINAL, 720, 440, 1, 0,
                            t_init, t_paint, t_key, t_mouse, t_tick, 0};
