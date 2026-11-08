/* AHCI (SATA) driver: polled, one command slot per port, DMA from
 * identity-mapped memory. Only ATA disks are used (ATAPI drives are skipped). */
#include "kernel.h"

#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define MAX_AHCI_PORTS 8

typedef struct {
    uintptr_t port;   /* port register block */
    uint32_t *cl;     /* command list (32 headers) */
    uint8_t *ct;      /* command table for slot 0 */
} ahci_port_t;
static ahci_port_t ports[MAX_AHCI_PORTS];
static int nports;
static blkdev_t *out_devs;
static int out_max, out_n;

static int wait_clear(uintptr_t reg, uint32_t mask, uint32_t ms) {
    uint32_t t = ticks_ms();
    while (R32(reg) & mask) { if (ticks_ms() - t > ms) return 0; delay_ms(1); }
    return 1;
}

static int port_cmd(ahci_port_t *ap, uint8_t command, uint32_t lba, int count, void *buf, int bytes, int write) {
    uintptr_t p = ap->port;
    if (!wait_clear(p + 0x20, 0x88, 1000)) return 0; /* BSY | DRQ */
    uint32_t *hdr = ap->cl;
    hdr[0] = 5 | (write ? 1 << 6 : 0) | ((bytes ? 1u : 0u) << 16);
    hdr[1] = 0;
    hdr[2] = (uint32_t)(uintptr_t)ap->ct;
    hdr[3] = 0;
    memset(ap->ct, 0, 0x80 + 16);
    uint8_t *f = ap->ct;
    f[0] = 0x27; /* host-to-device register FIS */
    f[1] = 0x80; /* command */
    f[2] = command;
    f[4] = lba; f[5] = lba >> 8; f[6] = lba >> 16;
    f[7] = 1 << 6; /* LBA mode */
    f[8] = lba >> 24;
    f[12] = count; f[13] = count >> 8;
    if (bytes) {
        uint32_t *prd = (uint32_t *)(ap->ct + 0x80);
        prd[0] = (uint32_t)(uintptr_t)buf;
        prd[1] = 0;
        prd[2] = 0;
        prd[3] = (uint32_t)(bytes - 1);
    }
    R32(p + 0x10) = 0xFFFFFFFF; /* clear interrupt status */
    R32(p + 0x38) = 1;          /* issue slot 0 */
    uint32_t t = ticks_ms();
    while (R32(p + 0x38) & 1) {
        if (R32(p + 0x10) & (1u << 30)) return 0; /* task file error */
        if (ticks_ms() - t > 5000) return 0;
    }
    return !(R32(p + 0x20) & 1) && !(R32(p + 0x10) & (1u << 30));
}

static int ahci_rw(blkdev_t *d, uint32_t lba, int count, void *buf, int write) {
    ahci_port_t *ap = &ports[d->priv];
    uint8_t *b = buf;
    while (count > 0) {
        int n = MIN(count, 128);
        if (!port_cmd(ap, write ? 0x35 : 0x25, lba, n, b, n * 512, write)) return 0;
        lba += n;
        count -= n;
        b += n * 512;
    }
    if (write && !port_cmd(ap, 0xEA, 0, 0, 0, 0, 0)) return 0; /* FLUSH CACHE EXT */
    return 1;
}

static int port_start(ahci_port_t *ap) {
    uintptr_t p = ap->port;
    R32(p + 0x18) &= ~1u; /* ST off */
    if (!wait_clear(p + 0x18, 1u << 15, 500)) return 0;
    R32(p + 0x18) &= ~(1u << 4); /* FRE off */
    if (!wait_clear(p + 0x18, 1u << 14, 500)) return 0;
    ap->cl = kmalloc_aligned(1024, 1024);
    uint8_t *fis = kmalloc_aligned(256, 256);
    ap->ct = kmalloc_aligned(256, 128);
    if (!ap->cl || !fis || !ap->ct) return 0;
    R32(p + 0x00) = (uint32_t)(uintptr_t)ap->cl;
    R32(p + 0x04) = 0;
    R32(p + 0x08) = (uint32_t)(uintptr_t)fis;
    R32(p + 0x0C) = 0;
    R32(p + 0x30) = 0xFFFFFFFF; /* SERR */
    R32(p + 0x10) = 0xFFFFFFFF; /* IS */
    R32(p + 0x14) = 0;          /* no interrupts: we poll */
    R32(p + 0x18) |= 1u << 4;   /* FRE */
    if (!wait_clear(p + 0x20, 0x88, 1000)) return 0;
    R32(p + 0x18) |= 1u;        /* ST */
    return 1;
}

static void found(pci_dev_t *pd) {
    uintptr_t abar = pci_bar_mem(pd, 5);
    if (!abar) return;
    pci_enable_mmio_dma(pd);
    /* BIOS/OS handoff, then make sure AHCI mode is on */
    if (R32(abar + 0x24) & 1) {
        R32(abar + 0x28) |= 2;
        uint32_t t = ticks_ms();
        while ((R32(abar + 0x28) & 1) && ticks_ms() - t < 2000) delay_ms(1);
    }
    R32(abar + 0x04) |= 1u << 31;
    uint32_t pi = R32(abar + 0x0C);
    for (int i = 0; i < 32 && nports < MAX_AHCI_PORTS && out_n < out_max; i++) {
        if (!(pi & (1u << i))) continue;
        uintptr_t p = abar + 0x100 + i * 0x80;
        uint32_t ssts = R32(p + 0x28);
        if ((ssts & 0xF) != 3 || ((ssts >> 8) & 0xF) != 1) continue; /* no device / not active */
        if (R32(p + 0x24) != 0x00000101) continue;                    /* not an ATA disk */
        ahci_port_t *ap = &ports[nports];
        ap->port = p;
        if (!port_start(ap)) { klog("ahci: port %d did not start\n", i); continue; }
        uint16_t *id = kmalloc_aligned(512, 64);
        if (!id || !port_cmd(ap, 0xEC, 0, 0, id, 512, 0)) { klog("ahci: identify failed on port %d\n", i); continue; }
        blkdev_t *d = &out_devs[out_n];
        memset(d, 0, sizeof *d);
        uint64_t sectors = (id[83] & (1 << 10)) ? (id[100] | ((uint64_t)id[101] << 16) | ((uint64_t)id[102] << 32)) : (id[60] | ((uint32_t)id[61] << 16));
        d->sectors = sectors > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)sectors;
        for (int k = 0; k < 20; k++) { d->model[k * 2] = id[27 + k] >> 8; d->model[k * 2 + 1] = id[27 + k] & 0xFF; }
        for (int k = 39; k >= 0 && (d->model[k] == ' ' || !d->model[k]); k--) d->model[k] = 0;
        d->rw = ahci_rw;
        d->priv = nports;
        klog("storage: AHCI port %d: %s, %u sectors\n", i, d->model, d->sectors);
        nports++;
        out_n++;
    }
}

int ahci_probe(blkdev_t *out, int max) {
    out_devs = out;
    out_max = max;
    out_n = 0;
    pci_find(0x01, 0x06, 0x01, found);
    return out_n;
}
