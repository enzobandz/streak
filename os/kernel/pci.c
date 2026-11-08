/* PCI configuration space access (mechanism #1) and device enumeration. */
#include "kernel.h"

uint32_t pci_read(int bus, int dev, int fn, int off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
void pci_write(int bus, int dev, int fn, int off, uint32_t v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}

/* Calls cb for every function whose class/subclass/prog-if match (-1 = any). */
void pci_find(int cls, int sub, int progif, void (*cb)(pci_dev_t *d)) {
    for (int bus = 0; bus < 256; bus++)
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id0 = pci_read(bus, dev, 0, 0);
            if ((id0 & 0xFFFF) == 0xFFFF) continue;
            int nfn = (pci_read(bus, dev, 0, 0x0C) >> 16) & 0x80 ? 8 : 1;
            for (int fn = 0; fn < nfn; fn++) {
                uint32_t id = pci_read(bus, dev, fn, 0);
                if ((id & 0xFFFF) == 0xFFFF) continue;
                uint32_t c = pci_read(bus, dev, fn, 0x08);
                int k = c >> 24, s = (c >> 16) & 0xFF, p = (c >> 8) & 0xFF;
                if ((cls >= 0 && k != cls) || (sub >= 0 && s != sub) || (progif >= 0 && p != progif)) continue;
                pci_dev_t d = {bus, dev, fn, id & 0xFFFF, id >> 16, k, s, p};
                cb(&d);
            }
        }
}

/* Memory BAR, mapped for access. BARs above 4 GiB go through the paging
 * window; 0 if it is an I/O BAR or cannot be mapped. We map 4 MiB, which
 * covers the register blocks of the controllers we drive. */
uintptr_t pci_bar_mem(pci_dev_t *d, int bar) {
    uint32_t lo = pci_read(d->bus, d->dev, d->fn, 0x10 + bar * 4);
    if (lo & 1) return 0;
    uint64_t phys = lo & ~0xFu;
    if (((lo >> 1) & 3) == 2) phys |= (uint64_t)pci_read(d->bus, d->dev, d->fn, 0x14 + bar * 4) << 32;
    if (!phys) return 0;
    return (uintptr_t)map_mmio(phys, phys >= 0x100000000ull ? 0x400000 : 0x1000);
}

void pci_enable_mmio_dma(pci_dev_t *d) {
    uint32_t cmd = pci_read(d->bus, d->dev, d->fn, 0x04);
    cmd |= (1 << 1) | (1 << 2); /* memory space, bus master */
    cmd |= (1 << 10);           /* INTx disable: drivers poll */
    pci_write(d->bus, d->dev, d->fn, 0x04, cmd);
}
