/* PS/2 keyboard and mouse, plus the VMware/QEMU/VirtualBox absolute pointer
 * (vmmouse) so the cursor follows the host mouse without capture in a VM. */
#include "kernel.h"

/* ---- keyboard ---- */
static void mouse_byte(uint8_t b);
static key_event_t kq[128];
static volatile int kq_head, kq_tail;
static int mods, extended;
static int caps_lock, num_lock = 1;
static volatile int leds_dirty = 1;

static const char map_lo[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v',
    'b','n','m',',','.','/', 0, '*', 0, ' ',
};
static const char map_hi[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
    'B','N','M','<','>','?', 0, '*', 0, ' ',
};

static void push_key(int k) {
    int n = (kq_head + 1) & 127;
    if (n == kq_tail) return;
    kq[kq_head].key = k;
    kq[kq_head].mods = mods;
    kq_head = n;
}

static void kbd_byte(uint8_t sc) {
    if (sc == 0xFA || sc == 0xFE) return; /* ACK / resend from LED updates */
    if (sc == 0xE0) { extended = 1; return; }
    int rel = sc & 0x80;
    sc &= 0x7F;
    if (extended) {
        extended = 0;
        if (sc == 0x1D) { mods = rel ? mods & ~MOD_CTRL : mods | MOD_CTRL; return; }
        if (sc == 0x38) { mods = rel ? mods & ~MOD_ALT : mods | MOD_ALT; return; }
        if (sc == 0x2A || sc == 0x36) return; /* fake shifts */
        if (rel) return;
        switch (sc) {
        case 0x48: push_key(KEY_UP); break;
        case 0x50: push_key(KEY_DOWN); break;
        case 0x4B: push_key(KEY_LEFT); break;
        case 0x4D: push_key(KEY_RIGHT); break;
        case 0x47: push_key(KEY_HOME); break;
        case 0x4F: push_key(KEY_END); break;
        case 0x49: push_key(KEY_PGUP); break;
        case 0x51: push_key(KEY_PGDN); break;
        case 0x53: push_key(KEY_DELETE); break;
        case 0x1C: push_key(KEY_ENTER); break;
        case 0x35: push_key('/'); break;
        case 0x5B: case 0x5C: push_key(KEY_SUPER); break;
        }
        return;
    }
    if (sc == 0x2A || sc == 0x36) { mods = rel ? mods & ~MOD_SHIFT : mods | MOD_SHIFT; return; }
    if (sc == 0x1D) { mods = rel ? mods & ~MOD_CTRL : mods | MOD_CTRL; return; }
    if (sc == 0x38) { mods = rel ? mods & ~MOD_ALT : mods | MOD_ALT; return; }
    if (rel) return;
    if (sc == 0x3A) { caps_lock = !caps_lock; leds_dirty = 1; return; }
    if (sc == 0x45) { num_lock = !num_lock; leds_dirty = 1; return; }
    if (sc == 0x46) return; /* scroll lock */
    if (sc == 0x37) { push_key('*'); return; } /* keypad * */
    if (sc >= 0x47 && sc <= 0x53) {
        /* keypad: digits with Num Lock on, navigation with it off */
        static const char digits[] = "789-456+1230.";
        static const int nav[] = {KEY_HOME, KEY_UP, KEY_PGUP, '-', KEY_LEFT, 0, KEY_RIGHT, '+', KEY_END, KEY_DOWN, KEY_PGDN, 0, KEY_DELETE};
        int i = sc - 0x47;
        if (digits[i] == '-' || digits[i] == '+') push_key(digits[i]);
        else if (num_lock && !(mods & MOD_SHIFT)) push_key(digits[i]);
        else if (nav[i]) push_key(nav[i]);
        return;
    }
    if (sc >= 0x3B && sc <= 0x44) { push_key(KEY_F1 + sc - 0x3B); return; }
    if (sc == 0x57) { push_key(KEY_F11); return; }
    if (sc == 0x58) { push_key(KEY_F12); return; }
    switch (sc) {
    case 0x01: push_key(KEY_ESC); return;
    case 0x0E: push_key(KEY_BACKSPACE); return;
    case 0x0F: push_key(KEY_TAB); return;
    case 0x1C: push_key(KEY_ENTER); return;
    }
    int shift = (mods & MOD_SHIFT) != 0;
    char lo = map_lo[sc];
    if (caps_lock && lo >= 'a' && lo <= 'z') shift = !shift; /* Caps Lock only affects letters */
    char c = shift ? map_hi[sc] : lo;
    if (c) push_key(c);
}

/* Send the lock-key LED state. Runs from the main loop with interrupts off so
 * the keyboard's ACK bytes can be consumed here instead of by the IRQ handler. */
static void update_leds(void) {
    leds_dirty = 0;
    uint8_t v = (caps_lock ? 4 : 0) | (num_lock ? 2 : 0);
    __asm__ volatile("pushf; cli");
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
        outb(0x60, pass == 0 ? 0xED : v);
        for (int i = 0; i < 200000; i++) {
            uint8_t st = inb(0x64);
            if (!(st & 1)) continue;
            uint8_t b = inb(0x60);
            if (st & 0x20) { mouse_byte(b); continue; }
            if (b == 0xFA || b == 0xFE) break;
            kbd_byte(b);
        }
    }
    __asm__ volatile("popf");
}

int keyboard_poll(key_event_t *ev) {
    if (leds_dirty) update_leds();
    if (kq_tail == kq_head) return 0;
    *ev = kq[kq_tail];
    kq_tail = (kq_tail + 1) & 127;
    return 1;
}
int keyboard_mods(void) { return mods; }

/* ---- PS/2 controller ---- */
static void ps2_wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {} }
static void ps2_wait_read(void) { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++) {} }
static void mouse_write(uint8_t v) {
    ps2_wait_write(); outb(0x64, 0xD4);
    ps2_wait_write(); outb(0x60, v);
    ps2_wait_read(); inb(0x60); /* ACK */
}

static void kbd_irq(regs_t *r) {
    (void)r;
    while (inb(0x64) & 1) {
        uint8_t st = inb(0x64);
        uint8_t b = inb(0x60);
        if (st & 0x20) mouse_byte(b);
        else kbd_byte(b);
    }
}

void keyboard_init(void) {
    while (inb(0x64) & 1) inb(0x60);
    irq_set_handler(1, kbd_irq);
}

/* ---- mouse ---- */
static mouse_event_t mq[256];
static volatile int mq_head, mq_tail;
static uint8_t packet[4];
static int pkt_idx, pkt_len = 3, vm_abs;

static void push_mouse(mouse_event_t *e) {
    int n = (mq_head + 1) & 255;
    if (n == mq_tail) return;
    mq[mq_head] = *e;
    mq_head = n;
}

#define VM_MAGIC 0x564D5868
#define VM_PORT 0x5658
static void vmcall(uint32_t cmd, uint32_t arg, uint32_t out[4]) {
    uint32_t a, b, c, d;
    __asm__ volatile("inl %%dx, %%eax"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(VM_MAGIC), "b"(arg), "c"(cmd), "d"(VM_PORT)
                     : "memory");
    out[0] = a; out[1] = b; out[2] = c; out[3] = d;
}

static int vmmouse_init(void) {
    uint32_t r[4];
    vmcall(10, 0, r); /* GETVERSION */
    if (r[1] != VM_MAGIC || r[0] == 0xFFFFFFFF) return 0;
    vmcall(41, 0x45414552, r); /* ABSPOINTER_COMMAND: enable */
    vmcall(40, 0, r);          /* status */
    if ((r[0] & 0xFFFF) == 0) return 0;
    vmcall(39, 1, r);          /* read id */
    if (r[0] != 0x3442554A) return 0;
    vmcall(41, 0x53424152, r); /* request absolute */
    return 1;
}

static void vmmouse_drain(void) {
    for (;;) {
        uint32_t r[4];
        vmcall(40, 0, r);
        if ((r[0] & 0xFFFF0000) == 0xFFFF0000) { vmcall(41, 0x45414552, r); return; }
        if ((r[0] & 0xFFFF) < 4) return;
        vmcall(39, 4, r);
        mouse_event_t e = {0};
        e.absolute = 1;
        e.ax = (int)r[1];
        e.ay = (int)r[2];
        e.wheel = (int8_t)(r[3] & 0xFF);
        if (r[0] & 0x20) e.buttons |= MB_LEFT;
        if (r[0] & 0x10) e.buttons |= MB_RIGHT;
        if (r[0] & 0x08) e.buttons |= MB_MIDDLE;
        push_mouse(&e);
    }
}

static void mouse_byte(uint8_t b) {
    /* PS/2 packets are decoded even with vmmouse on: some hosts keep sending
     * relative motion, and the absolute reports that follow override it. */
    if (pkt_idx == 0 && !(b & 0x08)) return; /* resync on the always-set bit 3 */
    packet[pkt_idx++] = b;
    if (pkt_idx < pkt_len) return;
    pkt_idx = 0;
    if (packet[0] & 0xC0) return; /* overflow */
    mouse_event_t e = {0};
    e.dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    e.dy = -((int)packet[2] - ((packet[0] & 0x20) ? 256 : 0));
    e.buttons = packet[0] & 7;
    if (pkt_len == 4) e.wheel = (int8_t)(packet[3] << 4) >> 4; /* 4-bit signed */
    push_mouse(&e);
}

static void mouse_irq(regs_t *r) {
    (void)r;
    while (inb(0x64) & 1) {
        uint8_t st = inb(0x64);
        uint8_t b = inb(0x60);
        if (st & 0x20) mouse_byte(b);
        else kbd_byte(b);
    }
    if (vm_abs) vmmouse_drain();
}

void mouse_init(void) {
    ps2_wait_write(); outb(0x64, 0xA8); /* enable aux port */
    ps2_wait_write(); outb(0x64, 0x20);
    ps2_wait_read(); uint8_t cfg = inb(0x60);
    cfg |= 0x03;  /* IRQ1 + IRQ12 */
    cfg &= ~0x20; /* mouse clock on */
    ps2_wait_write(); outb(0x64, 0x60);
    ps2_wait_write(); outb(0x60, cfg);
    mouse_write(0xF6); /* defaults */
    /* IntelliMouse handshake to get scroll-wheel packets */
    mouse_write(0xF3); mouse_write(200);
    mouse_write(0xF3); mouse_write(100);
    mouse_write(0xF3); mouse_write(80);
    ps2_wait_write(); outb(0x64, 0xD4);
    ps2_wait_write(); outb(0x60, 0xF2);
    ps2_wait_read(); inb(0x60);
    ps2_wait_read(); uint8_t id = inb(0x60);
    if (id == 3 || id == 4) pkt_len = 4;
    mouse_write(0xF3); mouse_write(100);
    mouse_write(0xF4); /* enable streaming */
    vm_abs = vmmouse_init();
    klog("mouse: ps/2 id=%u, absolute vmmouse=%d\n", id, vm_abs);
    irq_set_handler(12, mouse_irq);
}

int mouse_poll(mouse_event_t *ev) {
    if (mq_tail == mq_head) return 0;
    *ev = mq[mq_tail];
    mq_tail = (mq_tail + 1) & 255;
    return 1;
}
int mouse_is_absolute(void) { return vm_abs; }
