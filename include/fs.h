#ifndef ENIXNEL_FS_H
#define ENIXNEL_FS_H

#include <stdint.h>
#include <stddef.h>

/* Simple in-memory filesystem entry used by the CLI. */

#define ENIXNEL_MAX_FS_ENTRIES 128
#define ENIXNEL_MAX_NAME_LEN   31
#define ENIXNEL_MAX_FILE_SIZE  512

typedef struct fs_entry {
    uint8_t used;
    uint8_t is_dir;   /* 1 = directory, 0 = file */
    char    name[ENIXNEL_MAX_NAME_LEN + 1];

    /* File contents (only valid when is_dir == 0) */
    uint32_t size;
    char     data[ENIXNEL_MAX_FILE_SIZE];
} fs_entry_t;

/* Global table of entries, defined in crtfiles.c */
extern fs_entry_t fs_entries[ENIXNEL_MAX_FS_ENTRIES];

/* Lookup helper shared between create/delete code. Returns index or -1. */
int fs_find_index(const char* name);

/* Create APIs (implemented in kernel/crtfiles.c) */
int fs_create_dir(const char* name);
int fs_create_file(const char* name);

/* Delete APIs (implemented in kernel/delfiles.c) */
int fs_delete_dir(const char* name);
int fs_delete_file(const char* name);

/* File content APIs (implemented in kernel/crtfiles.c) */

/* Write data to file. If append != 0, append; otherwise overwrite.
 * Auto-creates the file if it does not exist. Returns 0 on success, <0 on error.
 */
int fs_write_file(const char* name, const char* data, size_t len, int append);

/* Read file contents. On success, *out_data points into internal storage and
 * *out_len is the length (not including a trailing '\0'). Returns 0 on success, <0 on error.
 */
int fs_read_file(const char* name, const char** out_data, size_t* out_len);

#endif /* ENIXNEL_FS_H */