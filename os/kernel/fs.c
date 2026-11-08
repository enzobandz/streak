/* Tiny in-memory file system shared by the Terminal and Notes apps. */
#include "kernel.h"

static file_t files[FS_MAX_FILES];

void fs_init(void) {
    const char *readme =
        "Welcome to StreakOS!\n\n"
        "Everything you see is drawn by a small kernel written from scratch:\n"
        "a compositing window manager, anti-aliased text and 60 fps animations.\n\n"
        "Tips\n"
        "  - Press the Windows key (or click the start button) to open Start.\n"
        "  - Type in Start to search apps, Enter to launch.\n"
        "  - Drag a window to the top edge to maximise, or to a side to snap.\n"
        "  - Alt+Tab switches windows, Alt+F4 closes one.\n"
        "  - Open Terminal and type 'help'.\n";
    fs_write("readme.txt", readme, strlen(readme));
    const char *notes = "My notes\n\nThis file is saved in memory as notes.txt.\n";
    fs_write("notes.txt", notes, strlen(notes));
}

file_t *fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && !strcmp(files[i].name, name)) return &files[i];
    return 0;
}

file_t *fs_write(const char *name, const char *data, size_t size) {
    file_t *f = fs_find(name);
    if (!f)
        for (int i = 0; i < FS_MAX_FILES && !f; i++)
            if (!files[i].used) f = &files[i];
    if (!f) return 0;
    char *copy = kmalloc(size + 1);
    if (!copy) return 0;
    memcpy(copy, data, size);
    copy[size] = 0;
    if (f->used) kfree(f->data);
    strlcpy(f->name, name, FS_NAME_LEN);
    f->data = copy;
    f->size = size;
    f->used = 1;
    return f;
}

int fs_remove(const char *name) {
    file_t *f = fs_find(name);
    if (!f) return 0;
    kfree(f->data);
    f->used = 0;
    return 1;
}

file_t *fs_list(int idx) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && idx-- == 0) return &files[i];
    return 0;
}
