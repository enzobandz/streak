/* USB: xHCI host controller driver with HID keyboards, mice and absolute
 * pointers (tablets), USB 2 hubs, and hot-plug.
 *
 * The controller is polled from the main loop (no interrupts). Rings and
 * contexts live in identity-mapped kernel memory, so their virtual addresses
 * are the physical addresses the controller uses.
 *
 * Keyboards use the HID boot protocol and are translated to PS/2 scancodes,
 * so they share the PS/2 keyboard's decoder (layout, Caps/Num Lock, shortcuts).
 * Mice and tablets are read in report protocol by parsing their HID report
 * descriptor (falling back to the boot protocol).
 *
 * Only USB 2 protocol ports are enumerated: keyboards, mice and the USB 2 half
 * of every hub live there. USB 3 ports carry only SuperSpeed devices.
 */
#include "wm.h"

#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))

enum {
    TRB_NORMAL = 1, TRB_SETUP = 2, TRB_DATA = 3, TRB_STATUS = 4, TRB_LINK = 6,
    TRB_ENABLE_SLOT = 9, TRB_DISABLE_SLOT = 10, TRB_ADDRESS = 11, TRB_CONFIG_EP = 12,
    TRB_EVAL_CTX = 13, TRB_RESET_EP = 14, TRB_SET_DEQ = 16,
    EV_TRANSFER = 32, EV_CMD_DONE = 33, EV_PORT = 34,
};
enum { CC_SUCCESS = 1, CC_STALL = 6, CC_SHORT = 13 };
enum { SPEED_FS = 1, SPEED_LS = 2, SPEED_HS = 3, SPEED_SS = 4 };
enum { HID_KBD = 1, HID_MOUSE_BOOT, HID_POINTER };

typedef struct { uint32_t d0, d1, d2, d3; } trb_t;
typedef struct { volatile trb_t *t; int n, idx, cycle; } ring_t;

typedef struct { int rid, off, size, sgn; int32_t lmin, lmax; int valid; } field_t;
typedef struct { int rid, absolute, ok; field_t x, y, wheel, btn[3]; } layout_t;

struct hc;
typedef struct hid {
    int active, kind, iface, dci, mps, errors, interval, rep_len;
    int needs_recover; /* endpoint halted: reset it from the top level of usb_poll */
    ring_t ring;
    uint8_t *buf;
    uint8_t prev[8];
    layout_t lay;
    uint8_t leds;
} hid_t;

#define MAX_DEV 32
#define MAX_HID 3
#define MAX_HUB_PORTS 16
typedef struct dev {
    int used, slot, speed, root_port, depth, tt_slot, tt_port, mps0;
    uint32_t route;
    struct hc *hc;
    ring_t ep0;
    uint8_t *in, *out;
    int is_hub, nports;
    struct dev *parent, *child[MAX_HUB_PORTS + 1];
    uint8_t fails[MAX_HUB_PORTS + 1];
    hid_t hid[MAX_HID];
    int nhid;
    uint16_t vid, pid;
} dev_t;

typedef struct hc {
    int ok;
    uintptr_t cap, op, rt, db;
    int max_slots, max_ports, csz;
    uint64_t *dcbaa;
    ring_t cmd;
    volatile trb_t *evr;
    int ev_idx, ev_cycle;
    uint8_t port_proto[256];
    dev_t *root[256];
    uint8_t fails[256]; /* enumeration attempts on a port; reset when it disconnects */
    volatile int cmd_done, xfer_done, xfer_slot;
    uint32_t cmd_ptr, cmd_ev[4], xfer_ev[4];
    uint8_t *ctl_buf;
    volatile int port_event;
} hc_t;

#define MAX_HC 4
#define EV_N 256
static hc_t hcs[MAX_HC];
static int nhc;
static dev_t devs[MAX_DEV];
static int announce; /* show toasts for hot-plug once the desktop is up */
static char status_text[96] = "No USB controller found";
int g_cmdline_nousb;

/* key repeat for USB keyboards (USB has no hardware typematic) */
static uint16_t rep_sc;
static uint32_t rep_at;

/* ---------------------------------------------------------------- rings */
static int ring_init(ring_t *r, int n) {
    if (!r->t) {
        r->t = kmalloc_aligned(n * sizeof(trb_t), 4096);
        if (!r->t) return 0;
    } else memset((void *)r->t, 0, n * sizeof(trb_t));
    r->n = n;
    r->idx = 0;
    r->cycle = 1;
    volatile trb_t *l = &r->t[n - 1];
    l->d0 = (uint32_t)(uintptr_t)r->t;
    l->d1 = 0;
    l->d2 = 0;
    l->d3 = (TRB_LINK << 10) | (1 << 1); /* toggle cycle */
    return 1;
}

static uint32_t ring_push(ring_t *r, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    volatile trb_t *t = &r->t[r->idx];
    t->d0 = d0;
    t->d1 = d1;
    t->d2 = d2;
    __asm__ volatile("" ::: "memory");
    t->d3 = (d3 & ~1u) | (uint32_t)r->cycle; /* cycle bit last: hands the TRB to the controller */
    uint32_t addr = (uint32_t)(uintptr_t)t;
    if (++r->idx == r->n - 1) {
        volatile trb_t *l = &r->t[r->n - 1];
        l->d3 = (l->d3 & ~1u) | (uint32_t)r->cycle;
        r->cycle ^= 1;
        r->idx = 0;
    }
    return addr;
}
static uint32_t ring_enqueue_ptr(ring_t *r) { return (uint32_t)(uintptr_t)&r->t[r->idx] | (uint32_t)r->cycle; }

static void doorbell(hc_t *h, int slot, int target) {
    __asm__ volatile("" ::: "memory");
    R32(h->db + 4 * slot) = target;
}

/* ---------------------------------------------------------------- events */
static void hid_event(dev_t *d, hid_t *x, int cc, int len);

static dev_t *dev_by_slot(hc_t *h, int slot) {
    for (int i = 0; i < MAX_DEV; i++)
        if (devs[i].used && devs[i].hc == h && devs[i].slot == slot) return &devs[i];
    return 0;
}

static void handle_event(hc_t *h, trb_t *e) {
    int type = (e->d3 >> 10) & 0x3F;
    if (type == EV_CMD_DONE) {
        if (e->d0 == h->cmd_ptr) {
            memcpy(h->cmd_ev, e, 16);
            h->cmd_done = 1;
        }
    } else if (type == EV_TRANSFER) {
        int slot = e->d3 >> 24, ep = (e->d3 >> 16) & 0x1F, cc = e->d2 >> 24;
        if (ep == 1) {
            if (slot == h->xfer_slot) { memcpy(h->xfer_ev, e, 16); h->xfer_done = 1; }
            return;
        }
        dev_t *d = dev_by_slot(h, slot);
        if (!d) return;
        for (int i = 0; i < d->nhid; i++)
            if (d->hid[i].active && d->hid[i].dci == ep) {
                hid_event(d, &d->hid[i], cc, d->hid[i].mps - (int)(e->d2 & 0xFFFFFF));
                return;
            }
    } else if (type == EV_PORT) {
        h->port_event = 1;
    }
}

static void poll_events(hc_t *h) {
    int any = 0;
    for (;;) {
        volatile trb_t *e = &h->evr[h->ev_idx];
        if ((e->d3 & 1) != (uint32_t)h->ev_cycle) break;
        trb_t ev = {e->d0, e->d1, e->d2, e->d3};
        if (++h->ev_idx == EV_N) { h->ev_idx = 0; h->ev_cycle ^= 1; }
        any = 1;
        handle_event(h, &ev);
    }
    if (any) {
        R32(h->rt + 0x20 + 0x18) = (uint32_t)(uintptr_t)&h->evr[h->ev_idx] | (1 << 3); /* ERDP, clear EHB */
        R32(h->rt + 0x20 + 0x1C) = 0;
    }
}

/* Issue a command and wait for its completion. Returns the completion code (1 = success). */
static int command(hc_t *h, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3, uint32_t out[4]) {
    h->cmd_done = 0;
    h->cmd_ptr = ring_push(&h->cmd, d0, d1, d2, d3);
    doorbell(h, 0, 0);
    uint32_t t = ticks_ms();
    while (!h->cmd_done) {
        poll_events(h);
        if (ticks_ms() - t > 1500) { klog("usb: command type %u timed out\n", (d3 >> 10) & 63); return -1; }
    }
    if (out) memcpy(out, h->cmd_ev, 16);
    return h->cmd_ev[2] >> 24;
}

/* ---------------------------------------------------------------- contexts */
static uint32_t *ctx_in(dev_t *d, int i) { return (uint32_t *)(d->in + d->hc->csz * i); } /* 0 = control, 1 = slot, 1+dci = endpoint */
static uint32_t *ctx_out(dev_t *d, int i) { return (uint32_t *)(d->out + d->hc->csz * i); } /* 0 = slot, dci = endpoint */

static void recover_ep(dev_t *d, int dci, ring_t *r) {
    hc_t *h = d->hc;
    command(h, 0, 0, 0, (TRB_RESET_EP << 10) | (dci << 16) | (d->slot << 24), 0);
    command(h, ring_enqueue_ptr(r), 0, 0, (TRB_SET_DEQ << 10) | (dci << 16) | (d->slot << 24), 0);
}

/* Control transfer on endpoint 0. Returns bytes transferred, or -1. */
static int control(dev_t *d, uint8_t type, uint8_t req, uint16_t value, uint16_t index, uint16_t len, void *data) {
    hc_t *h = d->hc;
    if (len > 1024) return -1;
    int in = (type & 0x80) != 0;
    if (!in && len) memcpy(h->ctl_buf, data, len);
    h->xfer_done = 0;
    h->xfer_slot = d->slot;
    int trt = len ? (in ? 3 : 2) : 0;
    ring_push(&d->ep0, type | (req << 8) | ((uint32_t)value << 16), index | ((uint32_t)len << 16), 8,
              (TRB_SETUP << 10) | (1 << 6) | (trt << 16));
    if (len) ring_push(&d->ep0, (uint32_t)(uintptr_t)h->ctl_buf, 0, len, (TRB_DATA << 10) | (in ? 1 << 16 : 0) | (1 << 2));
    ring_push(&d->ep0, 0, 0, 0, (TRB_STATUS << 10) | (1 << 5) | ((in && len) ? 0 : 1 << 16));
    doorbell(h, d->slot, 1);
    uint32_t t = ticks_ms();
    while (!h->xfer_done) {
        poll_events(h);
        if (ticks_ms() - t > 1000) { h->xfer_slot = 0; klog("usb: control %x/%x timed out\n", type, req); return -1; }
    }
    h->xfer_slot = 0;
    int cc = h->xfer_ev[2] >> 24;
    if (cc == CC_SHORT) {
        /* a short IN data stage stops before the status stage: finish it */
        int got = len - (int)(h->xfer_ev[2] & 0xFFFFFF);
        h->xfer_done = 0;
        h->xfer_slot = d->slot;
        t = ticks_ms();
        while (!h->xfer_done && ticks_ms() - t < 200) poll_events(h);
        h->xfer_slot = 0;
        if (in) memcpy(data, h->ctl_buf, MAX(got, 0));
        return MAX(got, 0);
    }
    if (cc != CC_SUCCESS) {
        if (cc == CC_STALL) recover_ep(d, 1, &d->ep0);
        return -1;
    }
    if (in && len) memcpy(data, h->ctl_buf, len);
    return len;
}

static int get_desc(dev_t *d, int type, int idx, void *buf, int len) {
    return control(d, 0x80, 6, (type << 8) | idx, 0, len, buf);
}

/* ---------------------------------------------------------------- HID */
static const uint16_t hid2sc[0x66] = {
    [0x04] = 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19,
    0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,             /* a..z */
    [0x1E] = 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,  /* 1..0 */
    [0x28] = 0x1C, 0x01, 0x0E, 0x0F, 0x39, 0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x2B, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, 0x3A,
    [0x3A] = 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58, /* F1..F12 */
    [0x47] = 0x46,
    [0x49] = 0xE052, 0xE047, 0xE049, 0xE053, 0xE04F, 0xE051, 0xE04D, 0xE04B, 0xE050, 0xE048,
    [0x53] = 0x45, 0xE035, 0x37, 0x4A, 0x4E, 0xE01C,
    [0x59] = 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x53,
    [0x64] = 0x2B, 0xE05D,
};
static const uint16_t mod2sc[8] = {0x1D, 0x2A, 0x38, 0xE05B, 0xE01D, 0x36, 0xE038, 0xE05C};

static void key_make(uint16_t sc) { input_scancode(sc); }
static void key_break(uint16_t sc) { input_scancode(sc | 0x80); }

static void kbd_report(hid_t *x, const uint8_t *r, int len) {
    if (len < 8) return;
    if (r[2] == 0x01) return; /* phantom state (rollover error) */
    uint8_t m = r[0], pm = x->prev[0];
    for (int b = 0; b < 8; b++) {
        if ((m ^ pm) & (1 << b)) { if (m & (1 << b)) key_make(mod2sc[b]); else key_break(mod2sc[b]); }
    }
    for (int i = 2; i < 8; i++) { /* released */
        uint8_t k = x->prev[i];
        if (k < 4 || k >= sizeof hid2sc / sizeof hid2sc[0] || !hid2sc[k]) continue;
        int still = 0;
        for (int j = 2; j < 8; j++) if (r[j] == k) still = 1;
        if (!still) { key_break(hid2sc[k]); if (rep_sc == hid2sc[k]) rep_sc = 0; }
    }
    for (int i = 2; i < 8; i++) { /* pressed */
        uint8_t k = r[i];
        if (k < 4 || k >= sizeof hid2sc / sizeof hid2sc[0] || !hid2sc[k]) continue;
        int was = 0;
        for (int j = 2; j < 8; j++) if (x->prev[j] == k) was = 1;
        if (was) continue;
        uint16_t sc = hid2sc[k];
        key_make(sc);
        if (sc != 0x3A && sc != 0x45 && sc != 0x46) { rep_sc = sc; rep_at = ticks_ms() + 500; } /* lock keys never repeat */
    }
    memcpy(x->prev, r, 8);
}

static int32_t field_get(const field_t *f, const uint8_t *r, int len) {
    int base = f->rid ? 8 : 0;
    uint32_t v = 0;
    for (int i = 0; i < f->size && i < 32; i++) {
        int bit = base + f->off + i;
        if (bit / 8 >= len) break;
        if (r[bit / 8] & (1 << (bit % 8))) v |= 1u << i;
    }
    if (f->sgn && f->size < 32 && (v & (1u << (f->size - 1)))) v |= ~0u << f->size;
    return (int32_t)v;
}

static void pointer_report(hid_t *x, const uint8_t *r, int len) {
    mouse_event_t e = {0};
    if (x->kind == HID_MOUSE_BOOT) {
        if (len < 3) return;
        e.buttons = r[0] & 7;
        e.dx = (int8_t)r[1];
        e.dy = (int8_t)r[2];
        if (len >= 4) e.wheel = -(int8_t)r[3];
        input_mouse(&e);
        return;
    }
    layout_t *L = &x->lay;
    if (L->rid && (len < 1 || r[0] != L->rid)) return;
    for (int b = 0; b < 3; b++) if (L->btn[b].valid && field_get(&L->btn[b], r, len)) e.buttons |= 1 << b;
    int32_t vx = field_get(&L->x, r, len), vy = field_get(&L->y, r, len);
    if (L->wheel.valid) e.wheel = -field_get(&L->wheel, r, len);
    if (L->absolute) {
        int32_t rx = L->x.lmax - L->x.lmin, ry = L->y.lmax - L->y.lmin;
        if (rx <= 0 || ry <= 0) return;
        e.absolute = 1;
        e.ax = (int)CLAMP((int64_t)(vx - L->x.lmin) * 65535 / rx, 0, 65535);
        e.ay = (int)CLAMP((int64_t)(vy - L->y.lmin) * 65535 / ry, 0, 65535);
    } else {
        e.dx = vx;
        e.dy = vy;
    }
    input_mouse(&e);
}

static void hid_queue(dev_t *d, hid_t *x) {
    ring_push(&x->ring, (uint32_t)(uintptr_t)x->buf, 0, x->mps, (TRB_NORMAL << 10) | (1 << 5) | (1 << 2));
    doorbell(d->hc, d->slot, x->dci);
}

static void hid_event(dev_t *d, hid_t *x, int cc, int len) {
    if (cc == CC_SUCCESS || cc == CC_SHORT) {
        x->errors = 0;
        len = CLAMP(len, 0, x->mps);
        if (x->kind == HID_KBD) kbd_report(x, x->buf, len);
        else pointer_report(x, x->buf, len);
        hid_queue(d, x);
    } else if (cc == CC_STALL || ++x->errors < 8) {
        /* Recovery issues controller commands; doing it here could nest inside
         * another command's wait, so it is deferred to usb_poll. */
        x->needs_recover = 1;
    } else {
        x->active = 0; /* device probably gone; the port scan will clean up */
    }
}

/* Minimal HID report descriptor parser: finds X, Y, wheel and three buttons. */
static void parse_report_desc(const uint8_t *p, int len, layout_t *L) {
    typedef struct { uint32_t page, rsize, rcount, rid; int32_t lmin, lmax; } glob_t;
    glob_t g = {0}, stack[4];
    int sp = 0;
    uint32_t usages[16];
    int nus = 0, have_min = 0, have_max = 0;
    uint32_t umin = 0, umax = 0;
    static int off[256];
    memset(off, 0, sizeof off);
    field_t cand[8];
    int ncand = 0, cand_kind[8], cand_abs[8];
    memset(L, 0, sizeof *L);
    for (int i = 0; i < len;) {
        uint8_t b = p[i++];
        if (b == 0xFE) { if (i < len) i += 2 + p[i]; continue; } /* long item */
        int sz = b & 3;
        if (sz == 3) sz = 4;
        if (i + sz > len) break;
        int type = (b >> 2) & 3, tag = b >> 4;
        uint32_t u = 0;
        for (int k = 0; k < sz; k++) u |= (uint32_t)p[i + k] << (8 * k);
        int32_t s = sz == 1 ? (int8_t)u : sz == 2 ? (int16_t)u : (int32_t)u;
        i += sz;
        if (type == 1) {
            switch (tag) {
            case 0: g.page = u; break;
            case 1: g.lmin = s; break;
            case 2: g.lmax = (g.lmin < 0) ? s : (int32_t)u; break;
            case 7: g.rsize = u; break;
            case 8: g.rid = u & 0xFF; break;
            case 9: g.rcount = u; break;
            case 10: if (sp < 4) stack[sp++] = g; break;
            case 11: if (sp > 0) g = stack[--sp]; break;
            }
        } else if (type == 2) {
            uint32_t full = sz == 4 ? u : (g.page << 16) | u;
            if (tag == 0 && nus < 16) usages[nus++] = full;
            else if (tag == 1) { umin = full; have_min = 1; }
            else if (tag == 2) { umax = full; have_max = 1; }
        } else if (type == 0) {
            if (tag == 8) { /* Input */
                for (uint32_t k = 0; k < g.rcount && k < 64; k++) {
                    uint32_t usage = 0;
                    if (nus) usage = usages[k < (uint32_t)nus ? k : (uint32_t)nus - 1];
                    else if (have_min && have_max && umin + k <= umax) usage = umin + k;
                    int kind = 0;
                    if (!(u & 1)) {
                        uint32_t pg = usage >> 16, id = usage & 0xFFFF;
                        if (pg == 1 && id == 0x30) kind = 1;
                        else if (pg == 1 && id == 0x31) kind = 2;
                        else if (pg == 1 && id == 0x38) kind = 3;
                        else if (pg == 9 && id >= 1 && id <= 3) kind = 3 + id;
                    }
                    if (kind && ncand < 8) {
                        field_t f = {(int)g.rid, off[g.rid], (int)g.rsize, g.lmin < 0, g.lmin, g.lmax, 1};
                        cand[ncand] = f;
                        cand_kind[ncand] = kind;
                        cand_abs[ncand] = !(u & 4);
                        ncand++;
                    }
                    off[g.rid] += g.rsize;
                }
            }
            nus = 0;
            have_min = have_max = 0;
        }
    }
    /* use the report that carries X */
    int rid = -1;
    for (int i = 0; i < ncand; i++) if (cand_kind[i] == 1) { rid = cand[i].rid; L->absolute = cand_abs[i]; break; }
    if (rid < 0) return;
    L->rid = rid;
    for (int i = 0; i < ncand; i++) {
        if (cand[i].rid != rid) continue;
        field_t *dst = cand_kind[i] == 1 ? &L->x : cand_kind[i] == 2 ? &L->y : cand_kind[i] == 3 ? &L->wheel : &L->btn[cand_kind[i] - 4];
        if (!dst->valid) *dst = cand[i];
    }
    L->ok = L->x.valid && L->y.valid;
}

/* ---------------------------------------------------------------- devices */
static int configure_endpoints(dev_t *d, uint32_t add_mask, int max_dci, int hub_ports, int ttt) {
    hc_t *h = d->hc;
    memset(d->in, 0, h->csz * 33);
    uint32_t *ctl = ctx_in(d, 0), *slot = ctx_in(d, 1), *oslot = ctx_out(d, 0);
    ctl[1] = 1 | add_mask;
    memcpy(slot, oslot, 16);
    slot[0] = (slot[0] & ~(0x1Fu << 27)) | ((uint32_t)MAX(max_dci, 1) << 27);
    if (hub_ports) {
        slot[0] |= 1 << 26;
        slot[1] = (slot[1] & 0x00FFFFFF) | ((uint32_t)hub_ports << 24);
        slot[2] = (slot[2] & ~(3u << 16)) | ((uint32_t)(ttt & 3) << 16);
    }
    slot[3] = 0;
    for (int i = 0; i < d->nhid; i++) {
        hid_t *x = &d->hid[i];
        if (!(add_mask & (1u << x->dci))) continue;
        uint32_t *ep = ctx_in(d, 1 + x->dci);
        ep[0] = (uint32_t)x->interval << 16;
        ep[1] = (3 << 1) | (7 << 3) | ((uint32_t)x->mps << 16); /* CErr 3, Interrupt IN */
        ep[2] = (uint32_t)(uintptr_t)x->ring.t | 1;
        ep[3] = 0;
        ep[4] = x->mps | ((uint32_t)x->mps << 16);
    }
    return command(h, (uint32_t)(uintptr_t)d->in, 0, 0, (TRB_CONFIG_EP << 10) | (d->slot << 24), 0) == CC_SUCCESS;
}

static int interval_for(int speed, int bInterval) {
    if (speed == SPEED_HS || speed == SPEED_SS) return CLAMP(bInterval - 1, 0, 15);
    int l = 0;
    while ((1 << (l + 1)) <= bInterval) l++;
    return CLAMP(l + 3, 3, 10);
}

static void remove_dev(dev_t *d);
static void hub_scan(dev_t *hub, int first);
static const char *speed_name(int s) { return s == SPEED_LS ? "low" : s == SPEED_FS ? "full" : s == SPEED_HS ? "high" : "super"; }

static dev_t *enumerate(hc_t *h, dev_t *parent, int pport, int speed, int root_port, uint32_t route, int depth) {
    uint32_t ev[4];
    if (command(h, 0, 0, 0, TRB_ENABLE_SLOT << 10, ev) != CC_SUCCESS) return 0;
    int slot = ev[3] >> 24;
    dev_t *d = 0;
    for (int i = 0; i < MAX_DEV && !d; i++) if (!devs[i].used) d = &devs[i];
    if (!d) { command(h, 0, 0, 0, (TRB_DISABLE_SLOT << 10) | (slot << 24), 0); return 0; }
    d->used = 0;
    /* keep DMA buffers allocated with the dev_t entry across reuse */
    uint8_t *in = d->in, *out = d->out;
    ring_t ep0 = d->ep0;
    hid_t hidkeep[MAX_HID];
    memcpy(hidkeep, d->hid, sizeof hidkeep);
    memset(d, 0, sizeof *d);
    d->in = in ? in : kmalloc_aligned(4096, 4096);
    d->out = out ? out : kmalloc_aligned(4096, 4096);
    d->ep0 = ep0;
    for (int i = 0; i < MAX_HID; i++) { d->hid[i].ring = hidkeep[i].ring; d->hid[i].buf = hidkeep[i].buf; }
    if (!d->in || !d->out || !ring_init(&d->ep0, 64)) { command(h, 0, 0, 0, (TRB_DISABLE_SLOT << 10) | (slot << 24), 0); return 0; }
    memset(d->in, 0, 4096);
    memset(d->out, 0, 4096);
    d->used = 1;
    d->hc = h;
    d->slot = slot;
    d->speed = speed;
    d->root_port = root_port;
    d->route = route;
    d->depth = depth;
    d->parent = parent;
    if (parent && (speed == SPEED_LS || speed == SPEED_FS)) {
        if (parent->speed == SPEED_HS) { d->tt_slot = parent->slot; d->tt_port = pport; }
        else { d->tt_slot = parent->tt_slot; d->tt_port = parent->tt_port; }
    }
    d->mps0 = speed == SPEED_SS ? 512 : speed == SPEED_HS ? 64 : 8;
    h->dcbaa[slot] = (uint32_t)(uintptr_t)d->out;

    uint32_t *ctl = ctx_in(d, 0), *sc = ctx_in(d, 1), *ep0c = ctx_in(d, 2);
    ctl[1] = 3; /* add slot + EP0 */
    sc[0] = route | ((uint32_t)speed << 20) | (1u << 27);
    sc[1] = (uint32_t)root_port << 16;
    sc[2] = d->tt_slot | (d->tt_port << 8);
    ep0c[1] = (3 << 1) | (4 << 3) | ((uint32_t)d->mps0 << 16);
    ep0c[2] = (uint32_t)(uintptr_t)d->ep0.t | 1;
    ep0c[4] = 8;
    if (command(h, (uint32_t)(uintptr_t)d->in, 0, 0, (TRB_ADDRESS << 10) | (slot << 24), 0) != CC_SUCCESS) {
        klog("usb: address device failed (port %d)\n", root_port);
        goto fail;
    }
    delay_ms(2);

    uint8_t desc[512];
    if (get_desc(d, 1, 0, desc, 8) < 8) goto fail;
    int mps = speed == SPEED_SS ? (1 << desc[7]) : desc[7];
    if (mps && mps != d->mps0) {
        d->mps0 = mps;
        memset(d->in, 0, h->csz * 3);
        ctl[1] = 2;
        ep0c[1] = (3 << 1) | (4 << 3) | ((uint32_t)mps << 16);
        ep0c[2] = (uint32_t)(uintptr_t)d->ep0.t | 1;
        ep0c[4] = 8;
        command(h, (uint32_t)(uintptr_t)d->in, 0, 0, (TRB_EVAL_CTX << 10) | (slot << 24), 0);
    }
    if (get_desc(d, 1, 0, desc, 18) < 18) goto fail;
    d->vid = desc[8] | (desc[9] << 8);
    d->pid = desc[10] | (desc[11] << 8);
    int dev_class = desc[4];
    if (get_desc(d, 2, 0, desc, 9) < 9) goto fail;
    int total = MIN(desc[2] | (desc[3] << 8), (int)sizeof desc);
    if (get_desc(d, 2, 0, desc, total) < 9) goto fail;
    int cfg_value = desc[5];

    /* walk the configuration: HID interfaces with an interrupt IN endpoint, or a hub */
    int cur_iface = -1, cur_class = 0, cur_sub = 0, cur_proto = 0, rep_len = 0, hub_ep = 0;
    for (int i = 0; i + 2 <= total && desc[i] >= 2; i += desc[i]) {
        uint8_t *p = desc + i;
        if (p[1] == 4 && p[0] >= 9) { cur_iface = p[2]; cur_class = p[5]; cur_sub = p[6]; cur_proto = p[7]; rep_len = 0; }
        else if (p[1] == 0x21 && p[0] >= 9) rep_len = p[7] | (p[8] << 8);
        else if (p[1] == 5 && p[0] >= 7 && (p[2] & 0x80) && (p[3] & 3) == 3) {
            if (cur_class == 9) hub_ep = 1;
            if (cur_class == 3 && d->nhid < MAX_HID) {
                int kind = cur_sub == 1 && cur_proto == 1 ? HID_KBD : cur_sub == 1 && cur_proto == 2 ? HID_MOUSE_BOOT : HID_POINTER;
                hid_t *x = &d->hid[d->nhid];
                int dup = 0;
                for (int k = 0; k < d->nhid; k++) if (d->hid[k].iface == cur_iface) dup = 1;
                if (dup) continue;
                x->kind = kind;
                x->iface = cur_iface;
                x->dci = (p[2] & 0x0F) * 2 + 1;
                x->mps = CLAMP(p[4] | ((p[5] & 7) << 8), 1, 1024); /* real endpoint size; buffer is 1 KiB */
                x->interval = interval_for(speed, p[6]);
                x->rep_len = rep_len;
                d->nhid++;
            }
        }
    }
    if (control(d, 0x00, 9, cfg_value, 0, 0, 0) < 0) goto fail; /* SET_CONFIGURATION */

    if (dev_class == 9 || hub_ep) {
        if (speed == SPEED_SS) { klog("usb: SuperSpeed hub ignored\n"); return d; }
        d->is_hub = 1;
        uint8_t hd[16];
        if (control(d, 0xA0, 6, 0x2900, 0, 8, hd) < 7) goto fail;
        d->nports = MIN(hd[2], MAX_HUB_PORTS);
        if (!configure_endpoints(d, 0, 1, d->nports, (hd[3] >> 5) & 3)) klog("usb: hub configure failed\n");
        for (int p = 1; p <= d->nports; p++) control(d, 0x23, 3, 8, p, 0, 0); /* SET_FEATURE PORT_POWER */
        delay_ms(MAX(hd[5] * 2, 100));
        klog("usb: hub %04x:%04x with %d ports (%s speed)\n", d->vid, d->pid, d->nports, speed_name(speed));
        hub_scan(d, 1);
        return d;
    }

    uint32_t mask = 0;
    int max_dci = 1;
    for (int i = 0; i < d->nhid; i++) {
        hid_t *x = &d->hid[i];
        int rlen = x->rep_len;
        memset(&x->lay, 0, sizeof x->lay);
        if (x->kind == HID_KBD) {
            control(d, 0x21, 0x0B, 0, x->iface, 0, 0); /* SET_PROTOCOL boot */
            control(d, 0x21, 0x0A, 0, x->iface, 0, 0); /* SET_IDLE 0 */
        } else {
            uint8_t rd[512];
            int got = rlen > 0 ? control(d, 0x81, 6, 0x2200, x->iface, MIN(rlen, (int)sizeof rd), rd) : -1;
            if (got > 0) parse_report_desc(rd, got, &x->lay);
            if (x->lay.ok) x->kind = HID_POINTER;
            else if (x->kind == HID_MOUSE_BOOT) control(d, 0x21, 0x0B, 0, x->iface, 0, 0);
            else { x->kind = 0; continue; } /* not a pointer we understand */
        }
        if (!x->buf) x->buf = kmalloc_aligned(1024, 64);
        if (!x->buf || !ring_init(&x->ring, 32)) continue;
        memset(x->prev, 0, sizeof x->prev);
        mask |= 1u << x->dci;
        max_dci = MAX(max_dci, x->dci);
    }
    if (mask && configure_endpoints(d, mask, max_dci, 0, 0)) {
        for (int i = 0; i < d->nhid; i++) {
            hid_t *x = &d->hid[i];
            if (!x->kind || !(mask & (1u << x->dci))) continue;
            x->active = 1;
            x->needs_recover = 0;
            hid_queue(d, x);
            klog("usb: %s on port %d (%s speed, %04x:%04x)\n",
                 x->kind == HID_KBD ? "keyboard" : x->kind == HID_MOUSE_BOOT ? "mouse (boot)" : (x->lay.absolute ? "tablet" : "mouse"),
                 root_port, speed_name(speed), d->vid, d->pid);
            if (announce) toast(x->kind == HID_KBD ? "USB keyboard connected" : "USB pointer connected", "Ready to use.");
        }
    }
    return d;
fail:
    klog("usb: enumeration failed on port %d\n", root_port);
    command(h, 0, 0, 0, (TRB_DISABLE_SLOT << 10) | (slot << 24), 0);
    h->dcbaa[slot] = 0;
    d->used = 0;
    return 0;
}

static void remove_dev(dev_t *d) {
    if (!d || !d->used) return;
    for (int p = 1; p <= MAX_HUB_PORTS; p++) if (d->child[p]) { remove_dev(d->child[p]); d->child[p] = 0; }
    int had_input = 0;
    for (int i = 0; i < d->nhid; i++) { if (d->hid[i].active && d->hid[i].kind == HID_KBD) rep_sc = 0; had_input |= d->hid[i].active; d->hid[i].active = 0; }
    command(d->hc, 0, 0, 0, (TRB_DISABLE_SLOT << 10) | (d->slot << 24), 0);
    d->hc->dcbaa[d->slot] = 0;
    d->used = 0;
    klog("usb: device %04x:%04x removed\n", d->vid, d->pid);
    if (announce && had_input) toast("USB device disconnected", "");
}

/* ---------------------------------------------------------------- hubs */
static void hub_port(dev_t *hub, int p) {
    uint8_t st[4];
    if (control(hub, 0xA3, 0, 0, p, 4, st) < 4) return;
    int status = st[0] | (st[1] << 8), change = st[2] | (st[3] << 8);
    if (change & 1) control(hub, 0x23, 1, 16, p, 0, 0); /* clear C_PORT_CONNECTION */
    int connected = status & 1;
    if (!connected || (change & 1)) {
        if (hub->child[p]) { remove_dev(hub->child[p]); hub->child[p] = 0; }
    }
    if (!connected) hub->fails[p] = 0;
    if (!connected || hub->child[p] || hub->fails[p] >= 3) return;
    hub->fails[p]++;
    control(hub, 0x23, 3, 4, p, 0, 0); /* SET_FEATURE PORT_RESET */
    uint32_t t = ticks_ms();
    do {
        delay_ms(10);
        if (control(hub, 0xA3, 0, 0, p, 4, st) < 4) return;
        status = st[0] | (st[1] << 8);
        change = st[2] | (st[3] << 8);
    } while (!(change & (1 << 4)) && ticks_ms() - t < 500);
    control(hub, 0x23, 1, 20, p, 0, 0); /* clear C_PORT_RESET */
    if (!(status & 2)) return;           /* not enabled */
    delay_ms(10);
    int speed = (status & (1 << 9)) ? SPEED_LS : (status & (1 << 10)) ? SPEED_HS : SPEED_FS;
    uint32_t route = hub->route | ((uint32_t)MIN(p, 15) << (4 * hub->depth));
    hub->child[p] = enumerate(hub->hc, hub, p, speed, hub->root_port, route, hub->depth + 1);
    if (hub->child[p]) hub->fails[p] = 0;
}

static void hub_scan(dev_t *hub, int first) {
    for (int p = 1; p <= hub->nports; p++) {
        if (!first) {
            uint8_t st[4];
            if (control(hub, 0xA3, 0, 0, p, 4, st) < 4) continue;
            int change = st[2] | (st[3] << 8), connected = st[0] & 1;
            if (!(change & 1) && ((connected != 0) == (hub->child[p] != 0) || hub->fails[p] >= 3)) continue;
        }
        hub_port(hub, p);
    }
}

/* ---------------------------------------------------------------- root ports */
#define PORT_KEEP ((1u << 0) | (1u << 3) | (0xFu << 5) | (1u << 9) | (0xFu << 10) | (3u << 14) | (1u << 24) | (7u << 25) | (1u << 30))
static volatile uint32_t *portsc(hc_t *h, int p) { return (volatile uint32_t *)(h->op + 0x400 + 0x10 * (p - 1)); }

static void root_port(hc_t *h, int p) {
    if (h->port_proto[p] == 3) return; /* SuperSpeed-only port */
    volatile uint32_t *sc = portsc(h, p);
    uint32_t v = *sc;
    if (v & (0x7Fu << 17)) *sc = (v & PORT_KEEP) | (v & (0x7Fu << 17)); /* acknowledge changes */
    int connected = v & 1;
    if ((!connected || (v & (1 << 17))) && h->root[p]) { remove_dev(h->root[p]); h->root[p] = 0; }
    if (!connected) h->fails[p] = 0;
    if (!connected || h->root[p] || h->fails[p] >= 3) return;
    h->fails[p]++;
    *sc = (*sc & PORT_KEEP) | (1 << 4); /* port reset */
    uint32_t t = ticks_ms();
    while (!(*sc & (1 << 21)) && ticks_ms() - t < 500) delay_ms(1);
    v = *sc;
    *sc = (v & PORT_KEEP) | (v & (0x7Fu << 17));
    if (!(v & 2)) { klog("usb: port %d did not enable\n", p); return; }
    delay_ms(10);
    int speed = (v >> 10) & 0xF;
    h->root[p] = enumerate(h, 0, 0, speed, p, 0, 0);
    if (h->root[p]) h->fails[p] = 0;
}

/* ---------------------------------------------------------------- controller */
static void hc_init(pci_dev_t *pd) {
    if (nhc >= MAX_HC) return;
    uintptr_t base = pci_bar_mem(pd, 0);
    if (!base) { klog("usb: xHCI registers could not be mapped\n"); return; }
    pci_enable_mmio_dma(pd);
    /* Intel 7/8/9-series: route the shared ports from EHCI to xHCI */
    static const uint16_t intel_switch[] = {0x1E31, 0x8C31, 0x9C31, 0x9CB1, 0x8CB1};
    if (pd->vendor == 0x8086)
        for (unsigned i = 0; i < sizeof intel_switch / sizeof intel_switch[0]; i++)
            if (pd->device == intel_switch[i]) {
                pci_write(pd->bus, pd->dev, pd->fn, 0xD8, pci_read(pd->bus, pd->dev, pd->fn, 0xDC));
                pci_write(pd->bus, pd->dev, pd->fn, 0xD0, pci_read(pd->bus, pd->dev, pd->fn, 0xD4));
            }
    hc_t *h = &hcs[nhc];
    memset(h, 0, sizeof *h);
    h->cap = base;
    h->op = base + (R32(base) & 0xFF);
    uint32_t hcs1 = R32(base + 4), hcs2 = R32(base + 8), hcc1 = R32(base + 0x10);
    h->rt = base + (R32(base + 0x18) & ~0x1Fu);
    h->db = base + (R32(base + 0x14) & ~0x3u);
    h->max_slots = MIN(hcs1 & 0xFF, 64);
    h->max_ports = (hcs1 >> 24) & 0xFF;
    h->csz = (hcc1 & 4) ? 64 : 32;

    /* extended capabilities: take ownership from the BIOS, learn port protocols */
    for (uint32_t off = ((hcc1 >> 16) & 0xFFFF) * 4; off;) {
        volatile uint32_t *c = (volatile uint32_t *)(base + off);
        uint32_t v = c[0];
        if ((v & 0xFF) == 1) {
            if (v & (1 << 16)) {
                c[0] = v | (1 << 24);
                uint32_t t = ticks_ms();
                while ((c[0] & (1 << 16)) && ticks_ms() - t < 1000) delay_ms(1);
                if (c[0] & (1 << 16)) { klog("usb: BIOS did not release xHCI, forcing\n"); c[0] = c[0] & ~(1u << 16); }
            }
            c[1] = (c[1] & ((7u << 1) | (0xFFu << 5) | (7u << 17))) | (7u << 29); /* disable SMIs, clear status */
        } else if ((v & 0xFF) == 2) {
            int major = v >> 24, first = c[2] & 0xFF, cnt = (c[2] >> 8) & 0xFF;
            for (int p = first; p < first + cnt && p < 256; p++) h->port_proto[p] = major;
        }
        uint32_t next = (v >> 8) & 0xFF;
        off = next ? off + next * 4 : 0;
    }

    /* stop and reset */
    R32(h->op) &= ~1u;
    uint32_t t = ticks_ms();
    while (!(R32(h->op + 4) & 1) && ticks_ms() - t < 100) delay_ms(1);
    R32(h->op) = 2;
    t = ticks_ms();
    while ((R32(h->op) & 2) && ticks_ms() - t < 1000) delay_ms(1);
    while ((R32(h->op + 4) & (1 << 11)) && ticks_ms() - t < 1000) delay_ms(1);
    if (R32(h->op + 4) & (1 << 11)) { klog("usb: controller not ready after reset\n"); return; }
    delay_ms(2);

    R32(h->op + 0x38) = h->max_slots;
    h->dcbaa = kmalloc_aligned(4096, 4096);
    h->ctl_buf = kmalloc_aligned(1024, 64);
    if (!h->dcbaa || !h->ctl_buf || !ring_init(&h->cmd, 256)) return;
    int nscratch = (((hcs2 >> 21) & 0x1F) << 5) | ((hcs2 >> 27) & 0x1F);
    if (nscratch) {
        uint32_t pagesize = (R32(h->op + 8) & 0xFFFF) << 12;
        uint64_t *arr = kmalloc_aligned(nscratch * 8, 64);
        if (!arr) return;
        for (int i = 0; i < nscratch; i++) {
            void *pg = kmalloc_aligned(pagesize, pagesize);
            if (!pg) return;
            arr[i] = (uint32_t)(uintptr_t)pg;
        }
        h->dcbaa[0] = (uint32_t)(uintptr_t)arr;
    }
    R32(h->op + 0x30) = (uint32_t)(uintptr_t)h->dcbaa;
    R32(h->op + 0x34) = 0;
    R32(h->op + 0x18) = (uint32_t)(uintptr_t)h->cmd.t | 1;
    R32(h->op + 0x1C) = 0;

    h->evr = kmalloc_aligned(EV_N * sizeof(trb_t), 4096);
    uint32_t *erst = kmalloc_aligned(64, 64);
    if (!h->evr || !erst) return;
    erst[0] = (uint32_t)(uintptr_t)h->evr;
    erst[1] = 0;
    erst[2] = EV_N;
    h->ev_idx = 0;
    h->ev_cycle = 1;
    uintptr_t ir = h->rt + 0x20;
    R32(ir + 0x08) = 1;
    R32(ir + 0x18) = (uint32_t)(uintptr_t)h->evr;
    R32(ir + 0x1C) = 0;
    R32(ir + 0x10) = (uint32_t)(uintptr_t)erst;
    R32(ir + 0x14) = 0;

    R32(h->op) = 1; /* run */
    t = ticks_ms();
    while ((R32(h->op + 4) & 1) && ticks_ms() - t < 100) delay_ms(1);
    if (R32(h->op + 4) & 1) { klog("usb: controller did not start\n"); return; }
    h->ok = 1;
    nhc++;
    klog("usb: xHCI %04x:%04x, %d ports, %d slots, %d-byte contexts\n", pd->vendor, pd->device, h->max_ports, h->max_slots, h->csz);

    for (int p = 1; p <= h->max_ports; p++) { /* power ports that need it */
        volatile uint32_t *sc = portsc(h, p);
        if (!(*sc & (1 << 9))) *sc = (*sc & PORT_KEEP) | (1 << 9);
    }
    delay_ms(100);
    for (int p = 1; p <= h->max_ports; p++) root_port(h, p);
}

static void found_xhci(pci_dev_t *d) { hc_init(d); }

void usb_init(void) {
    if (g_cmdline_nousb) { strlcpy(status_text, "USB driver disabled (nousb)", sizeof status_text); klog("usb: disabled by nousb\n"); return; }
    pci_find(0x0C, 0x03, 0x30, found_xhci);
    announce = 1;
}

const char *usb_status(void) {
    if (g_cmdline_nousb || !nhc) return status_text;
    int kb = 0, ptr = 0, hubs = 0;
    for (int i = 0; i < MAX_DEV; i++) {
        if (!devs[i].used) continue;
        hubs += devs[i].is_hub;
        for (int k = 0; k < devs[i].nhid; k++)
            if (devs[i].hid[k].active) { if (devs[i].hid[k].kind == HID_KBD) kb++; else ptr++; }
    }
    ksnprintf(status_text, sizeof status_text, "xHCI: %d keyboard%s, %d pointer%s, %d hub%s",
              kb, kb == 1 ? "" : "s", ptr, ptr == 1 ? "" : "s", hubs, hubs == 1 ? "" : "s");
    return status_text;
}

static void update_leds(void) {
    uint8_t leds = (uint8_t)input_lock_state();
    for (int i = 0; i < MAX_DEV; i++) {
        dev_t *d = &devs[i];
        if (!d->used) continue;
        for (int k = 0; k < d->nhid; k++) {
            hid_t *x = &d->hid[k];
            if (!x->active || x->kind != HID_KBD || x->leds == leds) continue;
            x->leds = leds;
            control(d, 0x21, 0x09, 0x0200, x->iface, 1, &leds); /* SET_REPORT (output) */
        }
    }
}

/* Called from the main loop: input events, key repeat, LEDs and hot-plug. */
void usb_poll(void) {
    if (!nhc) return;
    static uint32_t last_scan;
    for (int i = 0; i < nhc; i++) poll_events(&hcs[i]);
    for (int i = 0; i < MAX_DEV; i++) {
        dev_t *d = &devs[i];
        if (!d->used) continue;
        for (int k = 0; k < d->nhid; k++) {
            hid_t *x = &d->hid[k];
            if (!x->active || !x->needs_recover) continue;
            x->needs_recover = 0;
            recover_ep(d, x->dci, &x->ring);
            hid_queue(d, x);
        }
    }
    uint32_t now = ticks_ms();
    if (rep_sc && (int32_t)(now - rep_at) >= 0) { key_make(rep_sc); rep_at = now + 33; }
    update_leds();
    if (now - last_scan >= 250) {
        last_scan = now;
        for (int i = 0; i < nhc; i++) {
            hc_t *h = &hcs[i];
            for (int p = 1; p <= h->max_ports; p++) {
                uint32_t v = *portsc(h, p);
                if ((v & (1 << 17)) || (((v & 1) != 0) != (h->root[p] != 0) && h->fails[p] < 3)) root_port(h, p);
            }
            h->port_event = 0;
        }
        for (int i = 0; i < MAX_DEV; i++)
            if (devs[i].used && devs[i].is_hub) hub_scan(&devs[i], 0);
    }
}
