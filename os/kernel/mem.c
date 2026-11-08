/* First-fit heap with block coalescing over one physical memory region. */
#include "kernel.h"

typedef struct block { size_t size; int free; struct block *next, *prev; } block_t;
#define HDR ((sizeof(block_t) + 15) & ~15u)
static block_t *head;
static size_t total, used;

void heap_init(uintptr_t start, uintptr_t end) {
    start = (start + 15) & ~15u;
    head = (block_t *)start;
    head->size = end - start - HDR;
    head->free = 1;
    head->next = head->prev = 0;
    total = head->size;
    used = 0;
}

void *kmalloc(size_t n) {
    n = (n + 15) & ~15u;
    for (block_t *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + HDR + 64) {
            block_t *s = (block_t *)((uint8_t *)b + HDR + n);
            s->size = b->size - n - HDR;
            s->free = 1;
            s->next = b->next;
            s->prev = b;
            if (b->next) b->next->prev = s;
            b->next = s;
            b->size = n;
        }
        b->free = 0;
        used += b->size;
        return (uint8_t *)b + HDR;
    }
    klog("kmalloc: out of memory (%u bytes)\n", (unsigned)n);
    return 0;
}

void *kzalloc(size_t n) {
    void *p = kmalloc(n);
    if (p) memset(p, 0, (n + 3) & ~3u);
    return p;
}

void kfree(void *p) {
    if (!p) return;
    block_t *b = (block_t *)((uint8_t *)p - HDR);
    b->free = 1;
    used -= b->size;
    if (b->next && b->next->free) {
        b->size += HDR + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        block_t *a = b->prev;
        a->size += HDR + b->size;
        a->next = b->next;
        if (a->next) a->next->prev = a;
    }
}

size_t heap_used(void) { return used; }
size_t heap_total(void) { return total; }

/* Zeroed memory with the given power-of-two alignment, for DMA structures.
 * Memory is identity mapped (no paging), so virtual == physical. */
void *kmalloc_aligned(size_t n, size_t align) {
    uint8_t *p = kmalloc(n + align);
    if (!p) return 0;
    uintptr_t a = ((uintptr_t)p + align - 1) & ~(uintptr_t)(align - 1);
    memset((void *)a, 0, n);
    return (void *)a;
}
