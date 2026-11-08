/* Minimal freestanding C library pieces. */
#include "kernel.h"

void *memset(void *d, int c, size_t n) {
    uint8_t *p = d;
    if (n >= 16 && !((uintptr_t)p & 3) && !(n & 3)) {
        uint32_t v = (uint8_t)c * 0x01010101u;
        size_t cnt = n / 4;
        __asm__ volatile("rep stosl" : "+D"(p), "+c"(cnt) : "a"(v) : "memory");
        return d;
    }
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    void *dd = d;
    size_t words = n / 4, rest = n & 3;
    __asm__ volatile("rep movsl" : "+D"(dd), "+S"(s), "+c"(words) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(dd), "+S"(s), "+c"(rest) : : "memory");
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    uint8_t *dp = d;
    const uint8_t *sp = s;
    if (dp == sp || n == 0) return d;
    if (dp < sp || dp >= sp + n) return memcpy(d, s, n);
    while (n--) dp[n] = sp[n];
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a, *y = b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b) return (uint8_t)*a - (uint8_t)*b;
        if (!*a) return 0;
    }
    return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
void strlcpy(char *d, const char *s, size_t n) {
    if (!n) return;
    size_t i = 0;
    for (; i + 1 < n && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
char *strchr(const char *s, int c) {
    for (; *s; s++) if (*s == (char)c) return (char *)s;
    return c ? 0 : (char *)s;
}
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int str_startswith_ci(const char *s, const char *p) {
    for (; *p; s++, p++) if (tolower(*s) != tolower(*p)) return 0;
    return 1;
}
int str_contains_ci(const char *s, const char *needle) {
    if (!*needle) return 1;
    for (; *s; s++) if (str_startswith_ci(s, needle)) return 1;
    return 0;
}

/* ---- printf ---- */
typedef struct { char *buf; size_t n, pos; } outbuf_t;
static void put(outbuf_t *o, char c) {
    if (o->pos + 1 < o->n) o->buf[o->pos] = c;
    o->pos++;
}
static void put_num(outbuf_t *o, unsigned long long v, int base, int neg, int width, char pad, int upper) {
    char tmp[24];
    int i = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[i++] = dig[v % base]; v /= base; } while (v);
    if (neg) tmp[i++] = '-';
    while (i < width) tmp[i++] = pad;
    while (i) put(o, tmp[--i]);
}
int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    outbuf_t o = {buf, n, 0};
    for (; *fmt; fmt++) {
        if (*fmt != '%') { put(&o, *fmt); continue; }
        fmt++;
        char pad = ' ';
        int width = 0, ll = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l') { ll++; fmt++; }
        switch (*fmt) {
        case 'd': case 'i': {
            long long v = ll >= 2 ? va_arg(ap, long long) : va_arg(ap, int);
            put_num(&o, v < 0 ? -(unsigned long long)v : (unsigned long long)v, 10, v < 0, width, pad, 0);
            break;
        }
        case 'u': put_num(&o, ll >= 2 ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 10, 0, width, pad, 0); break;
        case 'x': put_num(&o, ll >= 2 ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 16, 0, width, pad, 0); break;
        case 'X': put_num(&o, ll >= 2 ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 16, 0, width, pad, 1); break;
        case 'p': put(&o, '0'); put(&o, 'x'); put_num(&o, (uintptr_t)va_arg(ap, void *), 16, 0, 8, '0', 0); break;
        case 'c': put(&o, (char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int len = (int)strlen(s);
            for (int i = len; i < width; i++) put(&o, ' ');
            while (*s) put(&o, *s++);
            break;
        }
        case '%': put(&o, '%'); break;
        default: put(&o, '%'); put(&o, *fmt); break;
        }
    }
    if (n) buf[o.pos < n ? o.pos : n - 1] = 0;
    return (int)o.pos;
}
int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- serial debug log (COM1) ---- */
void serial_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01); /* 115200 baud */
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}
static void serial_putc(char c) {
    for (int i = 0; i < 10000 && !(inb(0x3FD) & 0x20); i++) {}
    outb(0x3F8, c);
    outb(0xE9, c); /* QEMU/Bochs debug console */
}
void klog(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (char *p = buf; *p; p++) {
        if (*p == '\n') serial_putc('\r');
        serial_putc(*p);
    }
}

/* ---- doubles (calculator) ---- */
void fmt_double(char *buf, size_t n, double v) {
    if (v != v) { strlcpy(buf, "Error", n); return; }
    if (v > 1e15 || v < -1e15) { strlcpy(buf, v > 0 ? "Overflow" : "-Overflow", n); return; }
    int neg = v < 0;
    if (neg) v = -v;
    /* 10 significant decimals, trailing zeros trimmed */
    unsigned long long ip = (unsigned long long)v;
    double frac = v - (double)ip;
    unsigned long long f = (unsigned long long)(frac * 1e10 + 0.5);
    if (f >= 10000000000ULL) { ip++; f -= 10000000000ULL; }
    char fs[16];
    ksnprintf(fs, sizeof fs, "%010llu", f);
    int len = 10;
    while (len > 0 && fs[len - 1] == '0') len--;
    fs[len] = 0;
    if (ip == 0 && len == 0) neg = 0;
    if (len) ksnprintf(buf, n, "%s%llu.%s", neg ? "-" : "", ip, fs);
    else ksnprintf(buf, n, "%s%llu", neg ? "-" : "", ip);
}
double parse_double(const char *s, int *ok) {
    double v = 0, scale = 1;
    int neg = 0, any = 0, dot = 0;
    if (*s == '-') { neg = 1; s++; }
    for (; *s; s++) {
        if (*s == '.' && !dot) { dot = 1; continue; }
        if (*s < '0' || *s > '9') break;
        any = 1;
        if (dot) { scale /= 10; v += (*s - '0') * scale; }
        else v = v * 10 + (*s - '0');
    }
    if (ok) *ok = any && !*s;
    return neg ? -v : v;
}

/* ---- 64-bit division helpers normally supplied by libgcc ---- */
unsigned long long __udivmoddi4(unsigned long long n, unsigned long long d, unsigned long long *rem) {
    unsigned long long q = 0, r = 0;
    if (d == 0) { if (rem) *rem = 0; return 0; }
    if ((n >> 32) == 0 && (d >> 32) == 0) {
        uint32_t nn = (uint32_t)n, dd = (uint32_t)d;
        if (rem) *rem = nn % dd;
        return nn / dd;
    }
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ULL << i; }
    }
    if (rem) *rem = r;
    return q;
}
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return __udivmoddi4(n, d, 0); }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { unsigned long long r; __udivmoddi4(n, d, &r); return r; }
long long __divdi3(long long n, long long d) {
    int neg = (n < 0) ^ (d < 0);
    unsigned long long q = __udivdi3(n < 0 ? -(unsigned long long)n : (unsigned long long)n, d < 0 ? -(unsigned long long)d : (unsigned long long)d);
    return neg ? -(long long)q : (long long)q;
}
long long __moddi3(long long n, long long d) {
    unsigned long long r = __umoddi3(n < 0 ? -(unsigned long long)n : (unsigned long long)n, d < 0 ? -(unsigned long long)d : (unsigned long long)d);
    return n < 0 ? -(long long)r : (long long)r;
}
