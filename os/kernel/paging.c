/* PAE paging with 2 MiB pages.
 *
 * The low 4 GiB are identity mapped, so every pointer the kernel already uses
 * stays valid. A small window carved from the top of the heap's RAM range is
 * remapped to device memory above 4 GiB (64-bit PCI BARs, framebuffers),
 * which a 32-bit kernel could not otherwise reach. */
#include "kernel.h"

#define PAGE_2M 0x200000u
static uint64_t pdpt[4] __attribute__((aligned(32)));
static uint64_t pd[4][512] __attribute__((aligned(4096)));
static uintptr_t win_base, win_next, win_end;
static int paging_on;

int paging_init(uintptr_t window, size_t window_size) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    if (!(d & (1 << 6))) { klog("paging: CPU has no PAE\n"); return 0; }
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 512; j++) {
            uint64_t phys = ((uint64_t)i << 30) | ((uint64_t)j << 21);
            pd[i][j] = phys | 0x83; /* present, writable, 2 MiB */
        }
        pdpt[i] = (uint32_t)(uintptr_t)pd[i] | 1;
    }
    win_base = win_next = window;
    win_end = window + window_size;
    __asm__ volatile(
        "mov %0, %%cr3\n"
        "mov %%cr4, %%eax\n or $0x20, %%eax\n mov %%eax, %%cr4\n"   /* PAE */
        "mov %%cr0, %%eax\n or $0x80000000, %%eax\n mov %%eax, %%cr0\n" /* PG */
        :: "r"(pdpt) : "eax", "memory");
    paging_on = 1;
    return 1;
}

/* Returns a pointer through which `size` bytes at physical `phys` can be
 * accessed (uncached for device memory), or 0 if it cannot be mapped. */
void *map_mmio(uint64_t phys, size_t size) {
    if (phys + size <= 0x100000000ull) return (void *)(uintptr_t)phys;
    if (!paging_on) return 0;
    uint64_t start = phys & ~(uint64_t)(PAGE_2M - 1);
    uint64_t end = (phys + size + PAGE_2M - 1) & ~(uint64_t)(PAGE_2M - 1);
    size_t span = (size_t)(end - start);
    if (win_next + span > win_end) { klog("paging: MMIO window full\n"); return 0; }
    uintptr_t virt = win_next;
    for (uint64_t off = 0; off < span; off += PAGE_2M) {
        uintptr_t v = virt + (uintptr_t)off;
        pd[v >> 30][(v >> 21) & 511] = (start + off) | 0x83 | 0x18; /* + PWT|PCD: uncached */
    }
    win_next += span;
    __asm__ volatile("mov %%cr3, %%eax\n mov %%eax, %%cr3" ::: "eax", "memory"); /* flush TLB */
    return (void *)(virt + (uintptr_t)(phys - start));
}
