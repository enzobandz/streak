/* ATA PIO disk driver and StreakFS persistence.
 *
 * Safety: StreakOS only ever writes to a disk that already carries the
 * StreakFS signature, or to one whose first 64 sectors are entirely zero (a
 * brand-new virtual disk). Disks with any other content (MBR, GPT, NTFS, ...)
 * are never written. Every wait has a timeout, so missing or odd hardware
 * cannot hang the system; without a usable disk files simply stay in RAM.
 *
 * Layout (512-byte sectors):
 *   0          header: "STREAKFS", version, slot count, sectors per slot, data start
 *   1..8       directory: 32 entries x 128 bytes (name[32], size, used)
 *   64 + 128*i data for file slot i (up to 64 KiB each)
 */
#include "kernel.h"

#define SLOT_SECTORS 128
#define DATA_START 64
#define DISK_NEED (DATA_START + FS_MAX_FILES * SLOT_SECTORS)

typedef struct { uint16_t base, ctrl; int slave; uint32_t sectors; char model[41]; } ata_t;
static ata_t disk;
static int disk_ok;
static char status_text[96] = "Files are kept in memory only";

/* want_drq: also wait for DRQ and treat ERR/DF as failure (during a command).
 * Without it, only wait for BSY to clear: ERR may still be latched from an
 * earlier command and is reset by the next one. */
static int ata_wait(ata_t *d, int want_drq) {
    for (int i = 0; i < 2000000; i++) {
        uint8_t st = inb(d->base + 7);
        if (st == 0xFF) return 0;      /* no controller */
        if (st & 0x80) continue;       /* BSY */
        if (want_drq && (st & 0x21)) return 0; /* ERR / DF */
        if (!want_drq || (st & 0x08)) return 1;
    }
    return 0;
}
static void ata_delay(ata_t *d) { for (int i = 0; i < 4; i++) inb(d->ctrl); } /* ~400 ns */

static int ata_identify(ata_t *d) {
    if (inb(d->base + 7) == 0xFF) return 0;
    outb(d->ctrl, 0x02); /* nIEN: we poll */
    outb(d->base + 6, d->slave ? 0xB0 : 0xA0);
    ata_delay(d);
    outb(d->base + 2, 0); outb(d->base + 3, 0); outb(d->base + 4, 0); outb(d->base + 5, 0);
    outb(d->base + 7, 0xEC);
    ata_delay(d);
    uint8_t st = inb(d->base + 7);
    if (st == 0 || st == 0xFF) return 0;
    for (int i = 0; i < 1000000 && (inb(d->base + 7) & 0x80); i++) {}
    if (inb(d->base + 7) & 0x80) return 0;
    if (inb(d->base + 4) || inb(d->base + 5)) return 0; /* ATAPI (CD-ROM) or SATA bridge: skip */
    if (!ata_wait(d, 1)) return 0;
    uint16_t id[256];
    for (int i = 0; i < 256; i++) {
        uint16_t v;
        __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"((uint16_t)d->base));
        id[i] = v;
    }
    d->sectors = id[60] | ((uint32_t)id[61] << 16);
    for (int i = 0; i < 20; i++) { d->model[i * 2] = id[27 + i] >> 8; d->model[i * 2 + 1] = id[27 + i] & 0xFF; }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;
    return d->sectors > 0;
}

/* select the drive, wait until it is idle, then load LBA and count */
static int ata_select(ata_t *d, uint32_t lba, int count) {
    outb(d->base + 6, (d->slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    ata_delay(d);
    if (!ata_wait(d, 0)) return 0;
    outb(d->base + 2, count & 0xFF);
    outb(d->base + 3, lba & 0xFF);
    outb(d->base + 4, (lba >> 8) & 0xFF);
    outb(d->base + 5, (lba >> 16) & 0xFF);
    return 1;
}

static int ata_read(ata_t *d, uint32_t lba, int count, void *buf) {
    uint16_t *p = buf;
    while (count > 0) {
        int n = MIN(count, 128);
        if (!ata_select(d, lba, n)) return 0;
        outb(d->base + 7, 0x20);
        for (int s = 0; s < n; s++) {
            ata_delay(d);
            if (!ata_wait(d, 1)) return 0;
            for (int i = 0; i < 256; i++) {
                uint16_t v;
                __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"((uint16_t)d->base));
                *p++ = v;
            }
        }
        lba += n;
        count -= n;
    }
    return 1;
}

static int ata_write(ata_t *d, uint32_t lba, int count, const void *buf) {
    const uint16_t *p = buf;
    while (count > 0) {
        int n = MIN(count, 128);
        if (!ata_select(d, lba, n)) return 0;
        outb(d->base + 7, 0x30);
        for (int s = 0; s < n; s++) {
            ata_delay(d);
            if (!ata_wait(d, 1)) return 0;
            for (int i = 0; i < 256; i++) __asm__ volatile("outw %0, %1" ::"a"(*p++), "Nd"((uint16_t)d->base));
        }
        lba += n;
        count -= n;
    }
    if (!ata_wait(d, 0)) return 0;
    outb(d->base + 7, 0xE7); /* flush cache */
    ata_delay(d);
    if (!ata_wait(d, 0)) return 0;
    return !(inb(d->base + 7) & 0x21);
}

/* ---- StreakFS ---- */
typedef struct __attribute__((packed)) { char magic[8]; uint32_t version, slots, slot_sectors, data_start; } fs_header_t;
typedef struct __attribute__((packed)) { char name[FS_NAME_LEN]; uint32_t size, used; uint8_t pad[128 - FS_NAME_LEN - 8]; } fs_dirent_t;

static int is_blank(ata_t *d) {
    uint8_t *buf = kmalloc(64 * 512);
    if (!buf) return 0;
    int blank = ata_read(d, 0, 64, buf);
    for (int i = 0; blank && i < 64 * 512; i++) if (buf[i]) blank = 0;
    kfree(buf);
    return blank;
}
static int has_magic(ata_t *d) {
    uint8_t sec[512];
    if (!ata_read(d, 0, 1, sec)) return 0;
    fs_header_t *h = (fs_header_t *)sec;
    return !memcmp(h->magic, "STREAKFS", 8) && h->version == 1 && h->slots == FS_MAX_FILES &&
           h->slot_sectors == SLOT_SECTORS && h->data_start == DATA_START;
}

int storage_save_slot(int slot, const file_t *f) {
    if (!disk_ok) return 1;
    if (f && f->used && f->size) {
        int secs = (int)((f->size + 511) / 512);
        uint8_t *buf = kzalloc(secs * 512);
        if (!buf) return 0;
        memcpy(buf, f->data, f->size);
        int ok = ata_write(&disk, DATA_START + slot * SLOT_SECTORS, secs, buf);
        kfree(buf);
        if (!ok) goto fail;
    }
    uint8_t sec[512];
    uint32_t dsec = 1 + slot / 4;
    if (!ata_read(&disk, dsec, 1, sec)) goto fail;
    fs_dirent_t *e = (fs_dirent_t *)sec + (slot % 4);
    memset(e, 0, sizeof *e);
    if (f && f->used) { memcpy(e->name, f->name, FS_NAME_LEN); e->size = f->size; e->used = 1; }
    if (!ata_write(&disk, dsec, 1, sec)) goto fail;
    return 1;
fail:
    disk_ok = 0;
    strlcpy(status_text, "Disk error - files are now kept in memory only", sizeof status_text);
    klog("storage: write failed, disabling persistence\n");
    return 0;
}

/* Called by fs_init. Returns 1 if files were loaded from disk. */
int storage_init(file_t *files) {
    static const uint16_t bases[2] = {0x1F0, 0x170}, ctrls[2] = {0x3F6, 0x376};
    ata_t cand[4];
    int nc = 0;
    for (int b = 0; b < 2; b++)
        for (int sl = 0; sl < 2; sl++) {
            ata_t d = {bases[b], ctrls[b], sl, 0, {0}};
            if (ata_identify(&d)) {
                klog("storage: ATA %s %s: %s, %u sectors\n", b ? "secondary" : "primary", sl ? "slave" : "master", d.model, d.sectors);
                cand[nc++] = d;
            }
        }
    int pick = -1, format = 0;
    for (int i = 0; i < nc && pick < 0; i++) if (cand[i].sectors >= DISK_NEED && has_magic(&cand[i])) pick = i;
    for (int i = 0; i < nc && pick < 0; i++) if (cand[i].sectors >= DISK_NEED && is_blank(&cand[i])) { pick = i; format = 1; }
    if (pick < 0) {
        if (nc) strlcpy(status_text, "Files are kept in memory only (no blank or StreakOS disk found)", sizeof status_text);
        klog("storage: no usable disk, files stay in RAM\n");
        return 0;
    }
    disk = cand[pick];
    disk_ok = 1;
    if (format) {
        uint8_t sec[512];
        memset(sec, 0, sizeof sec);
        uint8_t *zero = kzalloc(8 * 512);
        if (!zero || !ata_write(&disk, 1, 8, zero)) { kfree(zero); disk_ok = 0; return 0; }
        kfree(zero);
        fs_header_t *h = (fs_header_t *)sec;
        memcpy(h->magic, "STREAKFS", 8);
        h->version = 1; h->slots = FS_MAX_FILES; h->slot_sectors = SLOT_SECTORS; h->data_start = DATA_START;
        if (!ata_write(&disk, 0, 1, sec)) { disk_ok = 0; return 0; }
        ksnprintf(status_text, sizeof status_text, "Saved to disk: %s (formatted)", disk.model[0] ? disk.model : "ATA disk");
        klog("storage: formatted blank disk\n");
        return 0; /* caller writes the default files, which persists them */
    }
    /* load the directory and file data */
    uint8_t dir[8 * 512];
    if (!ata_read(&disk, 1, 8, dir)) { disk_ok = 0; return 0; }
    int loaded = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_dirent_t *e = (fs_dirent_t *)dir + i;
        if (!e->used || e->size > FS_MAX_FILE) continue;
        int secs = (int)((e->size + 511) / 512);
        char *data = kmalloc(secs * 512 + 1);
        if (!data) continue;
        if (secs && !ata_read(&disk, DATA_START + i * SLOT_SECTORS, secs, data)) { kfree(data); continue; }
        data[e->size] = 0;
        memcpy(files[i].name, e->name, FS_NAME_LEN);
        files[i].name[FS_NAME_LEN - 1] = 0;
        files[i].data = data;
        files[i].size = e->size;
        files[i].used = 1;
        loaded++;
    }
    ksnprintf(status_text, sizeof status_text, "Saved to disk: %s", disk.model[0] ? disk.model : "ATA disk");
    klog("storage: loaded %d files from disk\n", loaded);
    return 1;
}

const char *storage_status(void) { return status_text; }
int storage_persistent(void) { return disk_ok; }
