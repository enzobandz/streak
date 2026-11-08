/* ACPI power control: soft-off (S5) and the reset register, with the
 * classic fallbacks. Tables are read in place (no paging, identity mapped). */
#include "kernel.h"

typedef struct __attribute__((packed)) { char sig[8]; uint8_t sum; char oem[6]; uint8_t rev; uint32_t rsdt; uint32_t len; uint64_t xsdt; } rsdp_t;
typedef struct __attribute__((packed)) { char sig[4]; uint32_t len; uint8_t rev, sum; char oem[6], table[8]; uint32_t oemrev, creator, crev; } sdt_t;

static uint32_t pm1a_cnt, pm1b_cnt, smi_cmd;
static uint8_t acpi_enable_val, reset_space, reset_val, reset_ok;
static uint64_t reset_addr;
static uint16_t slp_a, slp_b;
static int s5_ok;
static uintptr_t rsdp_hint;

void acpi_set_rsdp(uintptr_t p) { rsdp_hint = p; }

static int sum_ok(const void *p, uint32_t n) {
    uint8_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += ((const uint8_t *)p)[i];
    return s == 0;
}
static rsdp_t *find_rsdp(void) {
    if (rsdp_hint && !memcmp((void *)rsdp_hint, "RSD PTR ", 8)) return (rsdp_t *)rsdp_hint;
    volatile uint16_t *bda;
    __asm__("" : "=r"(bda) : "0"(0x40E)); /* hide the fixed BIOS data area address from the optimiser */
    uintptr_t ebda = (uintptr_t)*bda << 4;
    uintptr_t ranges[2][2] = {{ebda, ebda + 1024}, {0xE0000, 0x100000}};
    for (int r = 0; r < 2; r++) {
        if (ranges[r][0] < 0x80000 && r == 0) continue; /* implausible EBDA */
        for (uintptr_t p = ranges[r][0]; p < ranges[r][1]; p += 16)
            if (!memcmp((void *)p, "RSD PTR ", 8) && sum_ok((void *)p, 20)) return (rsdp_t *)p;
    }
    return 0;
}
static int addr_ok(uint64_t a) { return a && a < 0xFFFF0000ull; }

static void parse_s5(sdt_t *dsdt) {
    uint8_t *p = (uint8_t *)dsdt + sizeof(sdt_t), *end = (uint8_t *)dsdt + dsdt->len;
    for (; p + 8 < end; p++) {
        if (memcmp(p, "_S5_", 4)) continue;
        if (!(p[-1] == 0x08 || (p[-2] == 0x08 && p[-1] == '\\'))) continue; /* NameOp */
        if (p[4] != 0x12) continue;                                         /* PackageOp */
        uint8_t *q = p + 5;
        q += ((*q & 0xC0) >> 6) + 1; /* PkgLength */
        q++;                         /* NumElements */
        if (*q == 0x0A) q++;         /* BytePrefix */
        slp_a = *q++;
        if (*q == 0x0A) q++;
        slp_b = *q;
        s5_ok = 1;
        return;
    }
}

void acpi_init(void) {
    rsdp_t *r = find_rsdp();
    if (!r) { klog("acpi: no RSDP\n"); return; }
    int use_x = r->rev >= 2 && !r->rsdt && addr_ok(r->xsdt);
    sdt_t *root = (sdt_t *)(uintptr_t)(use_x ? r->xsdt : r->rsdt);
    if (!addr_ok((uintptr_t)root) || !sum_ok(root, root->len)) { klog("acpi: bad root table\n"); return; }
    int n = (root->len - sizeof(sdt_t)) / (use_x ? 8 : 4);
    for (int i = 0; i < n; i++) {
        uint64_t a = use_x ? ((uint64_t *)(root + 1))[i] : ((uint32_t *)(root + 1))[i];
        if (!addr_ok(a)) continue;
        sdt_t *t = (sdt_t *)(uintptr_t)a;
        if (memcmp(t->sig, "FACP", 4)) continue;
        uint8_t *f = (uint8_t *)t;
        uint64_t dsdt = *(uint32_t *)(f + 40);
        if (!dsdt && t->len >= 148) dsdt = *(uint64_t *)(f + 140);
        smi_cmd = *(uint32_t *)(f + 48);
        acpi_enable_val = f[52];
        pm1a_cnt = *(uint32_t *)(f + 64);
        pm1b_cnt = *(uint32_t *)(f + 68);
        if (t->len >= 129 && t->rev >= 2 && (*(uint32_t *)(f + 112) & (1 << 10))) {
            reset_space = f[116];
            reset_addr = *(uint64_t *)(f + 120);
            reset_val = f[128];
            reset_ok = 1;
        }
        if (addr_ok(dsdt) && !memcmp(((sdt_t *)(uintptr_t)dsdt)->sig, "DSDT", 4)) parse_s5((sdt_t *)(uintptr_t)dsdt);
    }
    klog("acpi: pm1a=%x s5=%d (%u/%u) reset=%d\n", pm1a_cnt, s5_ok, slp_a, slp_b, reset_ok);
}

void power_off(void) {
    klog("acpi: power off\n");
    cli();
    if (s5_ok && pm1a_cnt) {
        if (smi_cmd && acpi_enable_val && !(inw((uint16_t)pm1a_cnt) & 1)) {
            outb((uint16_t)smi_cmd, acpi_enable_val);
            for (int i = 0; i < 3000000 && !(inw((uint16_t)pm1a_cnt) & 1); i++) io_wait();
        }
        outw((uint16_t)pm1a_cnt, (slp_a << 10) | (1 << 13));
        if (pm1b_cnt) outw((uint16_t)pm1b_cnt, (slp_b << 10) | (1 << 13));
        for (int i = 0; i < 1000000; i++) io_wait();
    }
    outw(0x604, 0x2000);  /* QEMU */
    outw(0xB004, 0x2000); /* Bochs / older QEMU */
    outw(0x4004, 0x3400); /* VirtualBox */
}

void reboot(void) {
    cli();
    if (reset_ok) {
        if (reset_space == 1) outb((uint16_t)reset_addr, reset_val);
        else if (reset_space == 0 && addr_ok(reset_addr)) *(volatile uint8_t *)(uintptr_t)reset_addr = reset_val;
        for (int i = 0; i < 100000; i++) io_wait();
    }
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE); /* keyboard controller reset */
    for (int i = 0; i < 100000; i++) io_wait();
    outb(0xCF9, 0x02); /* PCI reset control */
    outb(0xCF9, 0x06);
    for (int i = 0; i < 100000; i++) io_wait();
    /* last resort: triple fault */
    static struct __attribute__((packed)) { uint16_t lim; uint32_t base; } none = {0, 0};
    __asm__ volatile("lidt %0; int3" ::"m"(none));
    for (;;) hlt();
}
