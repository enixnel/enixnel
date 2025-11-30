#include <stdint.h>
#include <stddef.h>
#include "fs.h"

/*
 * Deletion side of the simple in-memory "filesystem" for Enixnel v0.1.
 *
 * This is paired with kernel/crtfiles.c, which handles creation.
 *
 * Exposed API (declared in fs.h):
 *
 *   int fs_delete_dir(const char* name);
 *   int fs_delete_file(const char* name);
 *
 * Both return 0 on success, <0 on error.
 */

/*
 * Delete a directory entry with the given name.
 * Returns 0 on success, <0 on error (not found, or is a file).
 */
int fs_delete_dir(const char* name)
{
    int idx = fs_find_index(name);
    if (idx < 0) {
        return -1;  /* not found */
    }

    if (!fs_entries[idx].is_dir) {
        return -1;  /* exists but is a file, not a directory */
    }

    fs_entries[idx].used = 0;
    fs_entries[idx].is_dir = 0;
    fs_entries[idx].name[0] = '\0';

    return 0;
}

/*
 * Delete a file entry with the given name.
 * Returns 0 on success, <0 on error (not found, or is a directory).
 */
int fs_delete_file(const char* name)
{
    int idx = fs_find_index(name);
    if (idx < 0) {
        return -1;  /* not found */
    }

    if (fs_entries[idx].is_dir) {
        return -1;  /* exists but is a directory, not a file */
    }

    fs_entries[idx].used = 0;
    fs_entries[idx].is_dir = 0;
    fs_entries[idx].name[0] = '\0';

    return 0;
}