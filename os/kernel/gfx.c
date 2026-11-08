/* 2D rasteriser: anti-aliased shapes, alpha blending, rounded blits, soft
 * shadows, scaled blits for animations, and anti-aliased text. All targets are
 * opaque 0xFFRRGGBB surfaces; colours carry an alpha channel. */
#include "kernel.h"

rect_t g_clip = {-100000, -100000, 200000, 200000};

rect_t rect_intersect(rect_t a, rect_t b) {
    int x0 = MAX(a.x, b.x), y0 = MAX(a.y, b.y);
    int x1 = MIN(a.x + a.w, b.x + b.w), y1 = MIN(a.y + a.h, b.y + b.h);
    rect_t r = {x0, y0, x1 - x0, y1 - y0};
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
rect_t rect_union(rect_t a, rect_t b) {
    if (rect_empty(a)) return b;
    if (rect_empty(b)) return a;
    int x0 = MIN(a.x, b.x), y0 = MIN(a.y, b.y);
    int x1 = MAX(a.x + a.w, b.x + b.w), y1 = MAX(a.y + a.h, b.y + b.h);
    return mkrect(x0, y0, x1 - x0, y1 - y0);
}

surface_t *surface_new(int w, int h) {
    surface_t *s = kmalloc(sizeof *s);
    if (!s) return 0;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    s->px = kmalloc((size_t)w * h * 4);
    if (!s->px) { kfree(s); return 0; }
    s->w = w; s->h = h; s->stride = w;
    return s;
}
void surface_free(surface_t *s) { if (s) { kfree(s->px); kfree(s); } }
surface_t surface_view(surface_t *s, rect_t r) {
    r = rect_intersect(r, mkrect(0, 0, s->w, s->h));
    surface_t v = {s->px + r.y * s->stride + r.x, r.w, r.h, s->stride};
    return v;
}
void clip_reset(surface_t *s) { (void)s; g_clip = mkrect(-100000, -100000, 200000, 200000); }

static inline rect_t clip_for(surface_t *s, rect_t r) {
    return rect_intersect(rect_intersect(r, mkrect(0, 0, s->w, s->h)), g_clip);
}

static inline uint32_t blend_px(uint32_t d, uint32_t s, uint32_t a) { /* a: 0..256 */
    uint32_t rb = ((s & 0xFF00FF) * a + (d & 0xFF00FF) * (256 - a)) >> 8;
    uint32_t g = ((s & 0x00FF00) * a + (d & 0x00FF00) * (256 - a)) >> 8;
    return 0xFF000000u | (rb & 0xFF00FF) | (g & 0x00FF00);
}
uint32_t blend(uint32_t d, uint32_t s, int alpha) {
    uint32_t a = ((s >> 24) * (uint32_t)alpha) / 255;
    return blend_px(d, s, a + (a >> 7));
}
uint32_t mix(uint32_t a, uint32_t b, int t) { return blend_px(a, b, (uint32_t)t + ((uint32_t)t >> 7)); }

void fill_rect(surface_t *s, rect_t r, uint32_t c) {
    r = clip_for(s, r);
    if (rect_empty(r)) return;
    uint32_t a = c >> 24;
    for (int y = r.y; y < r.y + r.h; y++) {
        uint32_t *p = s->px + y * s->stride + r.x;
        if (a == 255) {
            int n = r.w;
            __asm__ volatile("rep stosl" : "+D"(p), "+c"(n) : "a"(c) : "memory");
        } else if (a) {
            uint32_t aa = a + (a >> 7);
            for (int i = 0; i < r.w; i++) p[i] = blend_px(p[i], c, aa);
        }
    }
}

void fill_vgradient(surface_t *s, rect_t r, uint32_t top, uint32_t bottom) {
    for (int y = 0; y < r.h; y++)
        fill_rect(s, mkrect(r.x, r.y + y, r.w, 1), (mix(top, bottom, r.h > 1 ? y * 255 / (r.h - 1) : 0) & 0x00FFFFFF) | (top & 0xFF000000));
}

/* ---- corner coverage tables ---- */
static uint8_t *corner_tab[33];
static float fsqrt(float v) { float r; __asm__("fsqrt" : "=t"(r) : "0"(v)); return r; }
static const uint8_t *corner_mask(int rad) {
    if (rad > 32) rad = 32;
    if (corner_tab[rad]) return corner_tab[rad];
    uint8_t *t = kmalloc(rad * rad + 1);
    for (int y = 0; y < rad; y++)
        for (int x = 0; x < rad; x++) {
            /* 4x4 supersampling for clean anti-aliased edges */
            int hit = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    float fx = x + (sx + 0.5f) / 4 - rad, fy = y + (sy + 0.5f) / 4 - rad;
                    if (fx * fx + fy * fy <= (float)rad * rad) hit++;
                }
            t[y * rad + x] = hit * 255 / 16;
        }
    corner_tab[rad] = t;
    return t;
}
/* coverage (0..255) of pixel (x,y) inside a w*h rounded rect */
static inline int rr_cov(const uint8_t *m, int rad, int x, int y, int w, int h) {
    int cx = x < rad ? x : (x >= w - rad ? w - 1 - x : -1);
    int cy = y < rad ? y : (y >= h - rad ? h - 1 - y : -1);
    if (cx < 0 || cy < 0) return 255;
    return m[cy * rad + cx];
}

void fill_round_rect(surface_t *s, rect_t r, int rad, uint32_t c) {
    rad = MIN(rad, MIN(r.w, r.h) / 2);
    if (rad <= 0) { fill_rect(s, r, c); return; }
    const uint8_t *m = corner_mask(rad);
    rect_t cl = clip_for(s, r);
    if (rect_empty(cl)) return;
    uint32_t ca = c >> 24;
    for (int y = cl.y; y < cl.y + cl.h; y++) {
        int ry = y - r.y;
        uint32_t *row = s->px + y * s->stride;
        if (ry >= rad && ry < r.h - rad) {
            fill_rect(s, mkrect(cl.x, y, cl.w, 1), c);
            continue;
        }
        for (int x = cl.x; x < cl.x + cl.w; x++) {
            int cov = rr_cov(m, rad, x - r.x, ry, r.w, r.h);
            if (!cov) continue;
            uint32_t a = ca * cov / 255;
            row[x] = blend_px(row[x], c, a + (a >> 7));
        }
    }
}

void stroke_round_rect(surface_t *s, rect_t r, int rad, uint32_t c) {
    rad = MIN(rad, MIN(r.w, r.h) / 2);
    const uint8_t *mo = rad > 0 ? corner_mask(rad) : 0;
    const uint8_t *mi = rad > 1 ? corner_mask(rad - 1) : 0;
    rect_t cl = clip_for(s, r);
    uint32_t ca = c >> 24;
    for (int y = cl.y; y < cl.y + cl.h; y++) {
        int ry = y - r.y;
        uint32_t *row = s->px + y * s->stride;
        for (int x = cl.x; x < cl.x + cl.w; x++) {
            int rx = x - r.x;
            int edge = rx < MAX(rad, 1) || rx >= r.w - MAX(rad, 1) || ry < MAX(rad, 1) || ry >= r.h - MAX(rad, 1);
            if (!edge) { x = r.x + r.w - MAX(rad, 1) - 1; continue; }
            int outer = rad > 0 ? rr_cov(mo, rad, rx, ry, r.w, r.h) : 255;
            int inner;
            if (rx < 1 || ry < 1 || rx >= r.w - 1 || ry >= r.h - 1) inner = 0;
            else inner = mi ? rr_cov(mi, rad - 1, rx - 1, ry - 1, r.w - 2, r.h - 2) : 255;
            int cov = outer - inner;
            if (cov <= 0) continue;
            uint32_t a = ca * cov / 255;
            row[x] = blend_px(row[x], c, a + (a >> 7));
        }
    }
}

void fill_circle(surface_t *s, int cx, int cy, int rad, uint32_t c) {
    if (rad < 1) rad = 1;
    rect_t cl = clip_for(s, mkrect(cx - rad - 1, cy - rad - 1, rad * 2 + 3, rad * 2 + 3));
    uint32_t ca = c >> 24;
    float in2 = (rad - 0.5f) * (rad - 0.5f), out2 = (rad + 0.5f) * (rad + 0.5f);
    uint32_t full = ca + (ca >> 7);
    for (int y = cl.y; y < cl.y + cl.h; y++) {
        uint32_t *row = s->px + y * s->stride;
        float dy = y + 0.5f - cy, dy2 = dy * dy;
        for (int x = cl.x; x < cl.x + cl.w; x++) {
            float dx = x + 0.5f - cx, d2 = dx * dx + dy2;
            if (d2 >= out2) continue;
            if (d2 <= in2) { row[x] = ca == 255 ? c : blend_px(row[x], c, full); continue; }
            float cov = rad + 0.5f - fsqrt(d2); /* only edge pixels need the root */
            uint32_t a = (uint32_t)(ca * cov);
            row[x] = blend_px(row[x], c, a + (a >> 7));
        }
    }
}

void draw_line(surface_t *s, int x0, int y0, int x1, int y1, int th, uint32_t c) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = MAX(ABS(dx), ABS(dy));
    int rad = MAX(th / 2, 1);
    int stride = MAX(rad / 2, 1);
    if (steps == 0) { fill_circle(s, x0, y0, rad, c); return; }
    for (int i = 0; i <= steps; i += stride)
        fill_circle(s, x0 + dx * i / steps, y0 + dy * i / steps, rad, c);
    fill_circle(s, x1, y1, rad, c);
}

/* ---- blits ---- */
void blit_opaque(surface_t *dst, int dx, int dy, surface_t *src, rect_t sr) {
    rect_t d = clip_for(dst, mkrect(dx, dy, sr.w, sr.h));
    if (rect_empty(d)) return;
    int ox = sr.x + d.x - dx, oy = sr.y + d.y - dy;
    for (int y = 0; y < d.h; y++)
        memcpy(dst->px + (d.y + y) * dst->stride + d.x, src->px + (oy + y) * src->stride + ox, d.w * 4);
}

void blit(surface_t *dst, int dx, int dy, surface_t *src, rect_t sr, int alpha) {
    if (alpha >= 255) { blit_opaque(dst, dx, dy, src, sr); return; }
    if (alpha <= 0) return;
    rect_t d = clip_for(dst, mkrect(dx, dy, sr.w, sr.h));
    if (rect_empty(d)) return;
    int ox = sr.x + d.x - dx, oy = sr.y + d.y - dy;
    uint32_t a = alpha + (alpha >> 7);
    for (int y = 0; y < d.h; y++) {
        uint32_t *dp = dst->px + (d.y + y) * dst->stride + d.x;
        uint32_t *sp = src->px + (oy + y) * src->stride + ox;
        for (int x = 0; x < d.w; x++) dp[x] = blend_px(dp[x], sp[x], a);
    }
}

void blit_rounded(surface_t *dst, int dx, int dy, surface_t *src, int rad, int alpha) {
    rad = MIN(rad, MIN(src->w, src->h) / 2);
    if (rad <= 0) { blit(dst, dx, dy, src, mkrect(0, 0, src->w, src->h), alpha); return; }
    const uint8_t *m = corner_mask(rad);
    rect_t d = clip_for(dst, mkrect(dx, dy, src->w, src->h));
    if (rect_empty(d)) return;
    for (int y = d.y; y < d.y + d.h; y++) {
        int sy = y - dy;
        uint32_t *dp = dst->px + y * dst->stride;
        uint32_t *sp = src->px + sy * src->stride - dx;
        if (sy >= rad && sy < src->h - rad) {
            /* middle rows: plain copy except when translucent */
            if (alpha >= 255) memcpy(dp + d.x, sp + d.x, d.w * 4);
            else {
                uint32_t a = alpha + (alpha >> 7);
                for (int x = d.x; x < d.x + d.w; x++) dp[x] = blend_px(dp[x], sp[x], a);
            }
            continue;
        }
        for (int x = d.x; x < d.x + d.w; x++) {
            int cov = rr_cov(m, rad, x - dx, sy, src->w, src->h);
            uint32_t a = (uint32_t)cov * alpha / 255;
            if (a >= 255) dp[x] = sp[x];
            else if (a) dp[x] = blend_px(dp[x], sp[x], a + (a >> 7));
        }
    }
}

void blit_scaled(surface_t *dst, rect_t dr, surface_t *src, int alpha, int rad) {
    if (dr.w <= 0 || dr.h <= 0 || alpha <= 0) return;
    rad = MIN(rad, MIN(dr.w, dr.h) / 2);
    const uint8_t *m = rad > 0 ? corner_mask(rad) : 0;
    rect_t d = clip_for(dst, dr);
    if (rect_empty(d)) return;
    uint32_t sx_step = ((uint32_t)src->w << 16) / dr.w;
    uint32_t sy_step = ((uint32_t)src->h << 16) / dr.h;
    for (int y = d.y; y < d.y + d.h; y++) {
        int ry = y - dr.y;
        int sy = (int)(((uint32_t)ry * sy_step) >> 16);
        if (sy >= src->h) sy = src->h - 1;
        uint32_t *sp = src->px + sy * src->stride;
        uint32_t *dp = dst->px + y * dst->stride;
        uint32_t fx = (uint32_t)(d.x - dr.x) * sx_step;
        for (int x = d.x; x < d.x + d.w; x++, fx += sx_step) {
            int sx = (int)(fx >> 16);
            if (sx >= src->w) sx = src->w - 1;
            int cov = m ? rr_cov(m, rad, x - dr.x, ry, dr.w, dr.h) : 255;
            uint32_t a = (uint32_t)cov * alpha / 255;
            if (a >= 255) dp[x] = sp[sx];
            else if (a) dp[x] = blend_px(dp[x], sp[sx], a + (a >> 7));
        }
    }
}

/* ---- soft drop shadow ---- */
#define SH_SPREAD 22
static uint8_t sh_lut[64][64];
static int sh_ready;
static void shadow_init(void) {
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            float d = fsqrt((float)(x * x + y * y));
            float t = 1.0f - d / SH_SPREAD;
            if (t < 0) t = 0;
            sh_lut[y][x] = (uint8_t)(255 * t * t * t);
        }
    sh_ready = 1;
}
/* Soft shadow of rounded rect r. Pixels inside `skip` (the window itself) are
 * not touched, everything else under or around r gets the falloff. */
void draw_shadow_ex(surface_t *dst, rect_t r, int rad, int strength, rect_t skip) {
    if (!sh_ready) shadow_init();
    rad = MIN(rad, MIN(r.w, r.h) / 2);
    rect_t ext = mkrect(r.x - SH_SPREAD, r.y - SH_SPREAD, r.w + 2 * SH_SPREAD, r.h + 2 * SH_SPREAD);
    rect_t cl = clip_for(dst, ext);
    if (rect_empty(cl)) return;
    int ix0 = r.x + rad, ix1 = r.x + r.w - 1 - rad; /* corner centres */
    int iy0 = r.y + rad, iy1 = r.y + r.h - 1 - rad;
    uint32_t full = (uint32_t)sh_lut[0][0] * strength / 255;
    for (int y = cl.y; y < cl.y + cl.h; y++) {
        uint32_t *row = dst->px + y * dst->stride;
        int dy = y < iy0 ? iy0 - y : (y > iy1 ? y - iy1 : 0);
        int in_skip_row = y >= skip.y && y < skip.y + skip.h;
        for (int x = cl.x; x < cl.x + cl.w; x++) {
            if (in_skip_row && x >= skip.x && x < skip.x + skip.w) { x = skip.x + skip.w - 1; continue; }
            int dx = x < ix0 ? ix0 - x : (x > ix1 ? x - ix1 : 0);
            uint32_t a;
            if (!dx && !dy) a = full;
            else {
                if (dx >= 64 || dy >= 64) continue;
                float dist = fsqrt((float)(dx * dx + dy * dy)) - rad;
                if (dist <= 0) a = full;
                else {
                    int di = (int)dist;
                    if (di >= SH_SPREAD) continue;
                    a = (uint32_t)sh_lut[0][di] * strength / 255;
                }
            }
            if (a) row[x] = blend_px(row[x], 0xFF000000, a);
        }
    }
}
void draw_shadow(surface_t *dst, rect_t r, int rad, int strength) { draw_shadow_ex(dst, r, rad, strength, r); }

/* ---- text ---- */
int draw_text_n(surface_t *s, int x, int y, const char *t, int n, const font_t *f, uint32_t c) {
    uint32_t ca = c >> 24;
    for (int i = 0; i < n && t[i]; i++) {
        int ch = (uint8_t)t[i];
        if (ch == '\t') { x += 4 * f->g[0].adv; continue; }
        if (ch < 32 || ch > 126) ch = '?';
        const glyph_t *g = &f->g[ch - 32];
        if (g->w) {
            int gx = x + g->x, gy = y + g->y;
            rect_t cl = clip_for(s, mkrect(gx, gy, g->w, g->h));
            const uint8_t *bits = f->bits + g->off;
            for (int yy = cl.y; yy < cl.y + cl.h; yy++) {
                uint32_t *row = s->px + yy * s->stride;
                const uint8_t *b = bits + (yy - gy) * g->w - gx;
                for (int xx = cl.x; xx < cl.x + cl.w; xx++) {
                    uint32_t a = b[xx];
                    if (!a) continue;
                    a = a * ca / 255;
                    row[xx] = blend_px(row[xx], c, a + (a >> 7));
                }
            }
        }
        x += g->adv;
    }
    return x;
}
int draw_text(surface_t *s, int x, int y, const char *t, const font_t *f, uint32_t c) {
    return draw_text_n(s, x, y, t, 1 << 30, f, c);
}
int text_width_n(const char *t, int n, const font_t *f) {
    int w = 0;
    for (int i = 0; i < n && t[i]; i++) {
        int ch = (uint8_t)t[i];
        if (ch == '\t') { w += 4 * f->g[0].adv; continue; }
        if (ch < 32 || ch > 126) ch = '?';
        w += f->g[ch - 32].adv;
    }
    return w;
}
int text_width(const char *t, const font_t *f) { return text_width_n(t, 1 << 30, f); }
void draw_text_centered(surface_t *s, rect_t r, const char *t, const font_t *f, uint32_t c) {
    draw_text(s, r.x + (r.w - text_width(t, f)) / 2, r.y + (r.h - f->height) / 2, t, f, c);
}

/* ---- blur (three box passes ~ gaussian) ---- */
static void blur_line(uint32_t *p, int n, int step, int rad, uint32_t *tmp) {
    uint32_t sr = 0, sg = 0, sb = 0;
    int win = rad * 2 + 1;
    for (int i = -rad; i <= rad; i++) {
        uint32_t v = p[CLAMP(i, 0, n - 1) * step];
        sr += (v >> 16) & 255; sg += (v >> 8) & 255; sb += v & 255;
    }
    for (int i = 0; i < n; i++) {
        tmp[i] = 0xFF000000u | ((sr / win) << 16) | ((sg / win) << 8) | (sb / win);
        uint32_t a = p[CLAMP(i - rad, 0, n - 1) * step], b = p[CLAMP(i + rad + 1, 0, n - 1) * step];
        sr += ((b >> 16) & 255) - ((a >> 16) & 255);
        sg += ((b >> 8) & 255) - ((a >> 8) & 255);
        sb += (b & 255) - (a & 255);
    }
    for (int i = 0; i < n; i++) p[i * step] = tmp[i];
}
void box_blur(surface_t *s, int rad) {
    uint32_t *tmp = kmalloc(MAX(s->w, s->h) * 4);
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < s->h; y++) blur_line(s->px + y * s->stride, s->w, 1, rad, tmp);
        for (int x = 0; x < s->w; x++) blur_line(s->px + x, s->h, s->stride, rad, tmp);
    }
    kfree(tmp);
}
