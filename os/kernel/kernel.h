/* StreakOS - shared kernel declarations. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "font.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ABS(a) ((a) < 0 ? -(a) : (a))

/* ---- port I/O ---- */
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0, %1" ::"a"(v), "Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0, %1" ::"a"(v), "Nd"(p)); }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* ---- libc-ish ---- */
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
void strlcpy(char *d, const char *s, size_t n);
char *strchr(const char *s, int c);
int ksnprintf(char *buf, size_t n, const char *fmt, ...);
int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int tolower(int c);
int str_startswith_ci(const char *s, const char *prefix);
int str_contains_ci(const char *s, const char *needle);
void fmt_double(char *buf, size_t n, double v);
double parse_double(const char *s, int *ok);
void serial_init(void);
void klog(const char *fmt, ...);

/* ---- memory ---- */
void heap_init(uintptr_t start, uintptr_t end);
void *kmalloc(size_t n);
void *kzalloc(size_t n);
void kfree(void *p);
size_t heap_used(void);
size_t heap_total(void);

/* ---- CPU / interrupts ---- */
typedef struct {
    uint32_t es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vec, err;
    uint32_t eip, cs, eflags;
} regs_t;
void gdt_init(void);
void idt_init(void);
void irq_set_handler(int irq, void (*h)(regs_t *));
void pic_unmask(int irq);

/* ---- time ---- */
void timer_init(int hz);
uint32_t ticks_ms(void);
typedef struct { int sec, min, hour, day, mon, year, wday; } rtc_time_t;
void rtc_read(rtc_time_t *t);
uint32_t rand32(void);
void cpu_name(char *out48);

/* ---- input ---- */
enum {
    KEY_NONE = 0,
    KEY_ESC = 0x100, KEY_BACKSPACE, KEY_TAB, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_DELETE, KEY_SUPER,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};
#define MOD_SHIFT 1
#define MOD_CTRL 2
#define MOD_ALT 4
typedef struct { int key; int mods; } key_event_t;
void keyboard_init(void);
int keyboard_poll(key_event_t *ev);
int keyboard_mods(void);

#define MB_LEFT 1
#define MB_RIGHT 2
#define MB_MIDDLE 4
typedef struct { int dx, dy, wheel, buttons, absolute, ax, ay; } mouse_event_t;
void mouse_init(void);
int mouse_poll(mouse_event_t *ev);
int mouse_is_absolute(void);

void acpi_set_rsdp(uintptr_t rsdp);
void acpi_init(void);
void power_off(void); /* returns if the machine could not be switched off */
void reboot(void);

/* ---- graphics ---- */
typedef struct { int x, y, w, h; } rect_t;
typedef struct { uint32_t *px; int w, h, stride; } surface_t;

static inline rect_t mkrect(int x, int y, int w, int h) { rect_t r = {x, y, w, h}; return r; }
rect_t rect_intersect(rect_t a, rect_t b);
rect_t rect_union(rect_t a, rect_t b);
static inline int rect_empty(rect_t r) { return r.w <= 0 || r.h <= 0; }
static inline int rect_contains(rect_t r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

surface_t *surface_new(int w, int h);
void surface_free(surface_t *s);
surface_t surface_view(surface_t *s, rect_t r);

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define WITH_ALPHA(c, a) (((c) & 0x00FFFFFFu) | ((uint32_t)(a) << 24))
uint32_t blend(uint32_t dst, uint32_t src, int alpha); /* src over dst with extra alpha */
uint32_t mix(uint32_t a, uint32_t b, int t);            /* t in 0..255 */

/* All drawing clips to the target surface and to the global clip rect (in target coords). */
extern rect_t g_clip;
void clip_reset(surface_t *s);
void fill_rect(surface_t *s, rect_t r, uint32_t argb);
void fill_round_rect(surface_t *s, rect_t r, int radius, uint32_t argb);
void stroke_round_rect(surface_t *s, rect_t r, int radius, uint32_t argb);
void fill_circle(surface_t *s, int cx, int cy, int radius, uint32_t argb);
void draw_line(surface_t *s, int x0, int y0, int x1, int y1, int thickness, uint32_t argb);
void fill_vgradient(surface_t *s, rect_t r, uint32_t top, uint32_t bottom);
void blit(surface_t *dst, int dx, int dy, surface_t *src, rect_t srect, int alpha);
void blit_opaque(surface_t *dst, int dx, int dy, surface_t *src, rect_t srect);
void blit_rounded(surface_t *dst, int dx, int dy, surface_t *src, int radius, int alpha);
void blit_scaled(surface_t *dst, rect_t drect, surface_t *src, int alpha, int radius);
void draw_shadow(surface_t *dst, rect_t r, int radius, int strength);
void draw_shadow_ex(surface_t *dst, rect_t r, int radius, int strength, rect_t skip);
int draw_text(surface_t *s, int x, int y, const char *text, const font_t *f, uint32_t argb);
int draw_text_n(surface_t *s, int x, int y, const char *text, int n, const font_t *f, uint32_t argb);
int text_width(const char *text, const font_t *f);
int text_width_n(const char *text, int n, const font_t *f);
void draw_text_centered(surface_t *s, rect_t r, const char *text, const font_t *f, uint32_t argb);
void box_blur(surface_t *s, int radius);

/* ---- display ---- */
typedef struct { uintptr_t addr; uint32_t pitch, width, height, bpp; int bgr; } fb_info_t;
void display_init(fb_info_t *fb);
extern surface_t *g_screen; /* back buffer */
extern int SCR_W, SCR_H;
void display_flush(rect_t r);

/* ---- animation ---- */
typedef struct { uint32_t start, dur; float from, to; int active; int ease; } anim_t;
enum { EASE_OUT_CUBIC, EASE_OUT_QUINT, EASE_OUT_BACK, EASE_IN_OUT, EASE_SPRING };
void anim_start(anim_t *a, float from, float to, uint32_t dur_ms, int ease);
float anim_value(anim_t *a, uint32_t now);
extern int g_anim_speed_pct; /* 100 = normal, 0 = animations off */

/* ---- ramfs ---- */
#define FS_MAX_FILES 32
#define FS_NAME_LEN 32
#define FS_MAX_FILE 65536
typedef struct { char name[FS_NAME_LEN]; char *data; size_t size; int used; } file_t;
void fs_init(void);
file_t *fs_find(const char *name);
file_t *fs_write(const char *name, const char *data, size_t size);
int fs_remove(const char *name);
file_t *fs_list(int idx);
file_t *fs_list_visible(int idx); /* skips dot-files such as .settings */
int storage_init(file_t *files);
int storage_save_slot(int slot, const file_t *f);
const char *storage_status(void);
int storage_persistent(void);

/* ---- clipboard (shared by Notes and Terminal) ---- */
void clipboard_set(const char *text, size_t len);
const char *clipboard_get(size_t *len);

/* ---- kernel misc ---- */
extern uint32_t g_frame_ms_hist[120];
extern int g_frame_hist_pos;
extern uint32_t g_fps;
