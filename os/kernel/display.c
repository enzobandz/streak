/* Linear framebuffer + full-screen back buffer. */
#include "kernel.h"

surface_t *g_screen;
int SCR_W, SCR_H;
static fb_info_t fb;

void display_init(fb_info_t *f) {
    fb = *f;
    SCR_W = fb.width;
    SCR_H = fb.height;
    g_screen = surface_new(SCR_W, SCR_H);
    memset(g_screen->px, 0, SCR_W * SCR_H * 4);
}

void display_flush(rect_t r) {
    r = rect_intersect(r, mkrect(0, 0, SCR_W, SCR_H));
    if (rect_empty(r)) return;
    for (int y = r.y; y < r.y + r.h; y++) {
        uint32_t *src = g_screen->px + y * g_screen->stride + r.x;
        uint8_t *dst = (uint8_t *)fb.addr + y * fb.pitch;
        if (fb.bpp == 32 && fb.bgr) {
            uint32_t *d = (uint32_t *)dst + r.x;
            for (int x = 0; x < r.w; x++) { uint32_t c = src[x]; d[x] = (c & 0xFF00FF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16); }
        } else if (fb.bpp == 32) {
            memcpy(dst + r.x * 4, src, r.w * 4);
        } else if (fb.bpp == 24) {
            uint8_t *d = dst + r.x * 3;
            for (int x = 0; x < r.w; x++, d += 3) {
                uint32_t c = src[x];
                d[0] = fb.bgr ? c >> 16 : c; d[1] = c >> 8; d[2] = fb.bgr ? c : c >> 16;
            }
        } else if (fb.bpp == 16) {
            uint16_t *d = (uint16_t *)dst + r.x;
            for (int x = 0; x < r.w; x++) {
                uint32_t c = src[x];
                d[x] = ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F);
            }
        }
    }
}
