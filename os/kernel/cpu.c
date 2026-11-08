/* GDT, IDT, 8259 PIC, PIT timer, CMOS clock, power control. */
#include "kernel.h"

/* ---- GDT ---- */
static uint64_t gdt[3];
static struct __attribute__((packed)) { uint16_t lim; uint32_t base; } gdtr, idtr;
extern void gdt_load(void *);
extern void idt_load(void *);

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00CF9A000000FFFFULL; /* ring0 code, flat 4 GiB */
    gdt[2] = 0x00CF92000000FFFFULL; /* ring0 data */
    gdtr.lim = sizeof gdt - 1;
    gdtr.base = (uint32_t)gdt;
    gdt_load(&gdtr);
}

/* ---- IDT + PIC ---- */
static struct __attribute__((packed)) { uint16_t lo, sel; uint8_t zero, flags; uint16_t hi; } idt[256];
extern uint32_t isr_table[48];
static void (*irq_handlers[16])(regs_t *);

static void pic_remap(void) {
    uint8_t m1 = 0xFF, m2 = 0xFF;
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, m1); outb(0xA1, m2);
}

void pic_unmask(int irq) {
    if (irq >= 8) {
        outb(0xA1, inb(0xA1) & ~(1 << (irq - 8)));
        irq = 2;
    }
    outb(0x21, inb(0x21) & ~(1 << irq));
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) {
        idt[i].lo = isr_table[i] & 0xFFFF;
        idt[i].hi = isr_table[i] >> 16;
        idt[i].sel = 0x08;
        idt[i].zero = 0;
        idt[i].flags = 0x8E;
    }
    idtr.lim = sizeof idt - 1;
    idtr.base = (uint32_t)idt;
    pic_remap();
    idt_load(&idtr);
}

void irq_set_handler(int irq, void (*h)(regs_t *)) { irq_handlers[irq] = h; pic_unmask(irq); }

static const char *exc_names[] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
    "Device not available", "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection fault", "Page fault", "Reserved", "x87 FPU error",
    "Alignment check", "Machine check", "SIMD FP error",
};
void panic_screen(const char *msg, regs_t *r);

void isr_dispatch(regs_t *r) {
    if (r->vec < 32) {
        const char *n = r->vec < 20 ? exc_names[r->vec] : "CPU exception";
        klog("EXCEPTION %u (%s) err=%x eip=%x\n", r->vec, n, r->err, r->eip);
        panic_screen(n, r);
        for (;;) { cli(); hlt(); }
    }
    int irq = r->vec - 32;
    if (irq_handlers[irq]) irq_handlers[irq](r);
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* ---- PIT ---- */
static volatile uint32_t ms;
static uint32_t ms_per_tick = 1;
static void timer_irq(regs_t *r) { (void)r; ms += ms_per_tick; }
void timer_init(int hz) {
    uint32_t div = 1193182 / hz;
    ms_per_tick = 1000 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, div >> 8);
    irq_set_handler(0, timer_irq);
}
uint32_t ticks_ms(void) { return ms; }

/* ---- random ---- */
static uint32_t rng_state = 0x9E3779B9;
uint32_t rand32(void) {
    rng_state ^= ms * 2654435761u;
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---- CMOS real-time clock ---- */
static uint8_t cmos(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
void rtc_read(rtc_time_t *t) {
    while (cmos(0x0A) & 0x80) {}
    uint8_t sec = cmos(0), min = cmos(2), hour = cmos(4), day = cmos(7), mon = cmos(8), year = cmos(9), wday = cmos(6);
    uint8_t b = cmos(0x0B);
    if (!(b & 0x04)) {
#define BCD(v) (((v) & 0x0F) + ((v) >> 4) * 10)
        sec = BCD(sec); min = BCD(min); day = BCD(day); mon = BCD(mon); year = BCD(year);
        hour = BCD(hour & 0x7F) | (hour & 0x80);
    }
    if (!(b & 0x02) && (hour & 0x80)) hour = ((hour & 0x7F) + 12) % 24;
    t->sec = sec; t->min = min; t->hour = hour; t->day = day; t->mon = mon; t->year = 2000 + year;
    t->wday = (wday + 6) % 7; /* 0 = Sunday */
}

void cpu_name(char *out) {
    uint32_t r[12];
    uint32_t max;
    __asm__ volatile("cpuid" : "=a"(max) : "a"(0x80000000) : "ebx", "ecx", "edx");
    if (max < 0x80000004) { strcpy(out, "x86 processor"); return; }
    for (int i = 0; i < 3; i++)
        __asm__ volatile("cpuid" : "=a"(r[i * 4]), "=b"(r[i * 4 + 1]), "=c"(r[i * 4 + 2]), "=d"(r[i * 4 + 3]) : "a"(0x80000002 + i));
    memcpy(out, r, 48);
    out[47] = 0;
    char *p = out;
    while (*p == ' ') p++;
    memmove(out, p, strlen(p) + 1);
}

/* ---- power ---- */
void power_off(void) {
    cli();
    outw(0x604, 0x2000);  /* QEMU (q35 / newer) */
    outw(0xB004, 0x2000); /* Bochs, older QEMU */
    outw(0x4004, 0x3400); /* VirtualBox */
    for (;;) hlt();
}
void reboot(void) {
    cli();
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE);
    for (;;) hlt();
}
