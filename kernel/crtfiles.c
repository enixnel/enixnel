#include <stdint.h>
#include <stddef.h>
#include "fs.h"

/*
 * Simple in-memory "filesystem" entries for Enixnel v0.1.
 *
 * This is NOT a real on-disk filesystem. It just lets the CLI
 * create and track a small set of named "directories" and "files"
 * in RAM so commands like:
 *
 *   crtdir foo
 *   cfile bar
 *
 * can behave in a useful way.
 */

/* Global table defined here, shared via fs.h */
fs_entry_t fs_entries[ENIXNEL_MAX_FS_ENTRIES];

/*
 * Common helper: simple strlen (since we are freestanding).
 */
static size_t fs_strlen(const char* s)
{
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (*s++) {
        ++n;
    }
    return n;
}

/*
 * Lookup helper shared between create/delete code.
 * Returns index or -1 if not found.
 */
int fs_find_index(const char* name)
{
    if (!name) {
        return -1;
    }

    for (int i = 0; i < ENIXNEL_MAX_FS_ENTRIES; ++i) {
        if (fs_entries[i].used) {
            const char* a = fs_entries[i].name;
            const char* b = name;
            int equal = 1;
            while (*a || *b) {
                if (*a != *b) {
                    equal = 0;
                    break;
                }
                ++a;
                ++b;
            }
            if (equal) {
                return i;
            }
        }
    }
    return -1;
}

/*
 * Internal helper: allocate a new entry slot.
 * Returns index or -1.
 */
static int fs_alloc_entry(const char* name, uint8_t is_dir)
{
    if (!name) {
        return -1;
    }

    size_t len = fs_strlen(name);
    if (len == 0 || len > ENIXNEL_MAX_NAME_LEN) {
        return -1;
    }

    /* Reject duplicates. */
    if (fs_find_index(name) >= 0) {
        return -1;
    }

    for (int i = 0; i < ENIXNEL_MAX_FS_ENTRIES; ++i) {
        if (!fs_entries[i].used) {
            fs_entries[i].used = 1;
            fs_entries[i].is_dir = is_dir;

            /* Manual strncpy to stay freestanding. */
            char* dst = fs_entries[i].name;
            const char* src = name;
            while (*src) {
                *dst++ = *src++;
            }
            *dst = '\0';

            /* Initialize file contents if this is a file entry. */
            fs_entries[i].size = 0;
            if (!is_dir) {
                fs_entries[i].data[0] = '\0';
            }

            return i;
        }
    }

    /* No free slots. */
    return -1;
}

/*
 * API for CLI:
 *
 *  fs_create_dir(name)  - returns 0 on success, <0 on error
 *  fs_create_file(name) - returns 0 on success, <0 on error
 *
 * The CLI code can call these and then decide what to print
 * based on the return value.
 */
int fs_create_dir(const char* name)
{
    int idx = fs_alloc_entry(name, 1 /* is_dir */);
    return (idx >= 0) ? 0 : -1;
}

int fs_create_file(const char* name)
{
    int idx = fs_alloc_entry(name, 0 /* is_dir */);
    return (idx >= 0) ? 0 : -1;
}

/*
 * File content APIs
 */

int fs_write_file(const char* name, const char* data, size_t len, int append)
{
    if (!name || (!data && len > 0)) {
        return -1;
    }

    int idx = fs_find_index(name);

    if (idx >= 0) {
        if (fs_entries[idx].is_dir) {
            /* Cannot write to a directory */
            return -1;
        }
    } else {
        /* Auto-create the file if it does not exist */
        idx = fs_alloc_entry(name, 0 /* is_dir */);
        if (idx < 0) {
            return -1;
        }
        fs_entries[idx].size = 0;
        fs_entries[idx].data[0] = '\0';
    }

    fs_entry_t* e = &fs_entries[idx];

    size_t offset = append ? e->size : 0;
    if (offset >= ENIXNEL_MAX_FILE_SIZE) {
        return -1;
    }

    size_t space = ENIXNEL_MAX_FILE_SIZE - offset;
    if (len > space) {
        len = space;
    }

    for (size_t i = 0; i < len; ++i) {
        e->data[offset + i] = data[i];
    }

    offset += len;
    e->size = offset;

    if (offset < ENIXNEL_MAX_FILE_SIZE) {
        e->data[offset] = '\0';
    } else {
        e->data[ENIXNEL_MAX_FILE_SIZE - 1] = '\0';
    }

    return 0;
}

int fs_read_file(const char* name, const char** out_data, size_t* out_len)
{
    if (!name) {
        return -1;
    }

    int idx = fs_find_index(name);
    if (idx < 0) {
        return -1;
    }

    fs_entry_t* e = &fs_entries[idx];
    if (e->is_dir) {
        return -1;
    }

    if (out_data) {
        *out_data = e->data;
    }
    if (out_len) {
        *out_len = e->size;
    }

    return 0;
}