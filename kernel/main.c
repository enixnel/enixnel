#include <stdint.h>
#include <stddef.h>
#include "fs.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

static size_t cursor_row = 0;
static size_t cursor_col = 0;
static uint8_t console_color = 0x07; /* light grey on black */

/* Track Shift key state for punctuation like '>' (Shift + '.') */
static int shift_down = 0;

/* Current working "directory" as a simple path prefix ("" = root). */
static char current_dir[ENIXNEL_MAX_NAME_LEN + 1] = "";

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void console_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t idx = y * VGA_WIDTH + x;
            VGA_MEMORY[idx] = vga_entry(' ', console_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void console_scroll(void)
{
    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    /* Move each row up by one */
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t from = y * VGA_WIDTH + x;
            const size_t to   = (y - 1) * VGA_WIDTH + x;
            VGA_MEMORY[to] = VGA_MEMORY[from];
        }
    }

    /* Clear last row */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t idx = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        VGA_MEMORY[idx] = vga_entry(' ', console_color);
    }

    cursor_row = VGA_HEIGHT - 1;
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
    }
}

static void console_putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        const size_t idx = cursor_row * VGA_WIDTH + cursor_col;
        VGA_MEMORY[idx] = vga_entry(c, console_color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT) {
        console_scroll();
    }
}

static void console_write(const char* s)
{
    while (*s) {
        console_putc(*s++);
    }
}

static void console_write_line(const char* s)
{
    console_write(s);
    console_putc('\n');
}

/* ---------- Low-level keyboard input (PS/2) ---------- */

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static char scancode_to_char(uint8_t sc)
{
    switch (sc) {
        /* Number row */
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';

        /* Top letter row */
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';

        /* Home row */
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';

        /* Bottom row */
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';

        /* Punctuation we care about */
        case 0x2B: return '\\';               /* backslash '\' */
        case 0x35: return '/';                /* forward slash '/' */
        case 0x34: return shift_down ? '>' : '.'; /* '.' or '>' */

        case 0x39: return ' ';   /* space */
        case 0x1C: return '\n';  /* enter */
        case 0x0E: return '\b';  /* backspace */

        default:
            return 0;
    }
}

static char keyboard_read_char(void)
{
    for (;;) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            uint8_t sc = inb(0x60);

            /* Handle Shift press/release explicitly */
            if (sc == 0x2A || sc == 0x36) {
                /* Left or Right Shift pressed */
                shift_down = 1;
                continue;
            }
            if (sc == 0xAA || sc == 0xB6) {
                /* Left or Right Shift released */
                shift_down = 0;
                continue;
            }

            /* Ignore other key releases */
            if (sc & 0x80) {
                continue;
            }

            char ch = scancode_to_char(sc);
            if (ch) {
                return ch;
            }
        }
    }
}

static void console_backspace(void)
{
    if (cursor_col == 0 && cursor_row == 0) {
        return;
    }
    if (cursor_col > 0) {
        cursor_col--;
    } else {
        cursor_row--;
        cursor_col = VGA_WIDTH - 1;
    }
    const size_t idx = cursor_row * VGA_WIDTH + cursor_col;
    VGA_MEMORY[idx] = vga_entry(' ', console_color);
}

static void console_read_line(char* buffer, size_t buflen)
{
    size_t len = 0;
    if (buflen == 0) {
        return;
    }

    for (;;) {
        char c = keyboard_read_char();

        if (c == '\n') {
            console_putc('\n');
            buffer[len] = '\0';
            return;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                console_backspace();
            }
        } else {
            if (len + 1 < buflen && c >= 32 && c < 127) {
                buffer[len++] = c;
                console_putc(c);
            }
        }
    }
}

/* ---------- Small string helpers for CLI parsing ---------- */

static size_t k_strlen(const char* s)
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

static int k_streq(const char* a, const char* b)
{
    if (!a || !b) {
        return 0;
    }
    while (*a || *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return 1;
}

/* ---------- Path helpers for simple hierarchical names ---------- */

/* Join dir and name into out. dir="" means root, so result is just name. */
static void path_join(const char* dir, const char* name, char* out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    size_t dlen = k_strlen(dir);
    size_t nlen = k_strlen(name);

    if (dlen == 0) {
        /* Just copy name */
        if (nlen >= out_size) {
            nlen = out_size - 1;
        }
        for (size_t i = 0; i < nlen; ++i) {
            out[i] = name[i];
        }
        out[nlen] = '\0';
        return;
    }

    /* dir + '/' + name */
    size_t total = dlen + 1 + nlen;
    if (total >= out_size) {
        /* Truncate safely */
        total = out_size - 1;
    }

    size_t pos = 0;
    for (size_t i = 0; i < dlen && pos < out_size - 1; ++i) {
        out[pos++] = dir[i];
    }
    if (pos < out_size - 1) {
        out[pos++] = '/';
    }
    for (size_t i = 0; i < nlen && pos < out_size - 1; ++i) {
        out[pos++] = name[i];
    }
    out[pos] = '\0';
}

/* Get parent directory of path into out. For "a/b/c" -> "a/b". For "a" -> "". */
static void path_parent(const char* path, char* out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (!path) {
        return;
    }

    size_t len = k_strlen(path);
    if (len == 0) {
        return;
    }

    /* Find last '/' */
    size_t last_slash = len;
    while (last_slash > 0 && path[last_slash - 1] != '/') {
        --last_slash;
    }

    if (last_slash == 0) {
        /* No '/' -> parent is root ("") */
        return;
    }

    /* Copy everything before the last '/' */
    size_t plen = last_slash - 1; /* exclude the '/' itself */
    if (plen >= out_size) {
        plen = out_size - 1;
    }

    for (size_t i = 0; i < plen; ++i) {
        out[i] = path[i];
    }
    out[plen] = '\0';
}

/* Get basename of path into out. For "a/b/c" -> "c", for "a" -> "a". */
static void path_basename(const char* path, char* out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (!path) {
        return;
    }

    size_t len = k_strlen(path);
    if (len == 0) {
        return;
    }

    /* Find last '/' */
    size_t start = len;
    while (start > 0 && path[start - 1] != '/') {
        --start;
    }

    /* Copy from start to end */
    size_t blen = len - start;
    if (blen >= out_size) {
        blen = out_size - 1;
    }

    for (size_t i = 0; i < blen; ++i) {
        out[i] = path[start + i];
    }
    out[blen] = '\0';
}

/* Split line into command and the rest of the line as args_ptr. */
static void cli_split_command(const char* line, char* cmd_out, size_t cmd_out_size, const char** args_ptr)
{
    const char* p = line;

    /* Skip leading spaces */
    while (*p == ' ') {
        ++p;
    }

    /* Copy command token */
    size_t cmd_len = 0;
    while (*p && *p != ' ' && cmd_len + 1 < cmd_out_size) {
        cmd_out[cmd_len++] = *p++;
    }
    cmd_out[cmd_len] = '\0';

    /* Skip spaces before args */
    while (*p == ' ') {
        ++p;
    }

    *args_ptr = p;
}

/* Get first argument token from args string into arg_out. */
static void cli_first_arg(const char* args, char* arg_out, size_t arg_out_size)
{
    const char* p = args;
    size_t len = 0;

    /* Skip leading spaces */
    while (*p == ' ') {
        ++p;
    }

    while (*p && *p != ' ' && len + 1 < arg_out_size) {
        arg_out[len++] = *p++;
    }
    arg_out[len] = '\0';
}

/* ---------- CLI command handlers ---------- */

/* Initialize a simple default filesystem layout:
 *   /bin   - holds entries representing built-in commands
 *   /user  - default "home" directory for the user
 */
static void fs_init_layout(void)
{
    /* Root-level directories */
    fs_create_dir("bin");
    fs_create_dir("user");

    /* Represent built-in commands as files under /bin (purely cosmetic) */
    fs_create_file("bin/echo");
    fs_create_file("bin/crtdir");
    fs_create_file("bin/cfile");
    fs_create_file("bin/deldir");
    fs_create_file("bin/dfile");
    fs_create_file("bin/sdir");
    fs_create_file("bin/sfile");
    fs_create_file("bin/efile");
    fs_create_file("bin/clr");
    fs_create_file("bin/cd");
}

static void cli_cmd_help(void)
{
    console_write_line("Available commands:");
    console_write_line("  help              - show this help");
    console_write_line("  echo <text>       - print text");
    console_write_line("  crtdir <name>     - create directory");
    console_write_line("  cfile <name>      - create file");
    console_write_line("  deldir <name>     - delete directory");
    console_write_line("  dfile <name>      - delete file");
    console_write_line("  sdir              - list entries in current directory");
    console_write_line("  sfile <name>      - show file contents");
    console_write_line("  efile <expr>      - edit file (efile text > file, efile text >> file)");
    console_write_line("  clr               - clear the screen");
    console_write_line("  cd <name>         - change directory (.. for parent)");
}

static void cli_cmd_echo(const char* args)
{
    if (!args || *args == '\0') {
        console_putc('\n');
        return;
    }
    console_write(args);
    console_putc('\n');
}

static void cli_cmd_crtdir(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char full[ENIXNEL_MAX_NAME_LEN + 1];

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("crtdir: missing name");
        return;
    }

    path_join(current_dir, name, full, sizeof(full));

    int rc = fs_create_dir(full);
    if (rc == 0) {
        console_write("Directory created: ");
        console_write_line(name);
    } else {
        console_write("crtdir: failed to create ");
        console_write_line(name);
    }
}

static void cli_cmd_cfile(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char full[ENIXNEL_MAX_NAME_LEN + 1];

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("cfile: missing name");
        return;
    }

    path_join(current_dir, name, full, sizeof(full));

    int rc = fs_create_file(full);
    if (rc == 0) {
        console_write("File created: ");
        console_write_line(name);
    } else {
        console_write("cfile: failed to create ");
        console_write_line(name);
    }
}

static void cli_cmd_deldir(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char full[ENIXNEL_MAX_NAME_LEN + 1];

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("deldir: missing name");
        return;
    }

    path_join(current_dir, name, full, sizeof(full));

    int rc = fs_delete_dir(full);
    if (rc == 0) {
        console_write("Directory deleted: ");
        console_write_line(name);
    } else {
        console_write("deldir: failed to delete ");
        console_write_line(name);
    }
}

static void cli_cmd_dfile(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char full[ENIXNEL_MAX_NAME_LEN + 1];

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("dfile: missing name");
        return;
    }

    path_join(current_dir, name, full, sizeof(full));

    int rc = fs_delete_file(full);
    if (rc == 0) {
        console_write("File deleted: ");
        console_write_line(name);
    } else {
        console_write("dfile: failed to delete ");
        console_write_line(name);
    }
}

static void cli_cmd_sdir(void)
{
    int any = 0;
    char parent[ENIXNEL_MAX_NAME_LEN + 1];
    char base[ENIXNEL_MAX_NAME_LEN + 1];

    for (int i = 0; i < ENIXNEL_MAX_FS_ENTRIES; ++i) {
        if (!fs_entries[i].used) {
            continue;
        }

        path_parent(fs_entries[i].name, parent, sizeof(parent));
        path_basename(fs_entries[i].name, base, sizeof(base));

        if (!k_streq(parent, current_dir)) {
            continue;
        }

        any = 1;
        if (fs_entries[i].is_dir) {
            console_write("[DIR]  ");
        } else {
            console_write("[FILE] ");
        }
        console_write_line(base);
    }

    if (!any) {
        console_write_line("sdir: no entries");
    }
}

static void cli_cmd_clr(void)
{
    console_clear();
}

static void cli_cmd_sfile(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char full[ENIXNEL_MAX_NAME_LEN + 1];
    const char* data = 0;
    size_t len = 0;

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("sfile: missing name");
        return;
    }

    path_join(current_dir, name, full, sizeof(full));

    if (fs_read_file(full, &data, &len) != 0) {
        console_write("sfile: no such file: ");
        console_write_line(name);
        return;
    }

    for (size_t i = 0; i < len; ++i) {
        console_putc(data[i]);
    }
    console_putc('\n');
}

/* efile syntax: efile text > file.txt  (overwrite)
 *                efile text >> file.txt (append)
 */
static void cli_cmd_efile(const char* args)
{
    if (!args) {
        console_write_line("efile: missing arguments");
        return;
    }

    const char* p = args;

    /* Skip leading spaces for text start */
    while (*p == ' ') {
        ++p;
    }
    const char* text_start = p;

    /* Find first '>' */
    const char* redir = 0;
    while (*p) {
        if (*p == '>') {
            redir = p;
            break;
        }
        ++p;
    }
    if (!redir) {
        console_write_line("efile: missing '>'");
        return;
    }

    /* Text ends before '>' (trim trailing spaces) */
    const char* text_end = redir;
    while (text_end > text_start && text_end[-1] == ' ') {
        --text_end;
    }

    char text[ENIXNEL_MAX_FILE_SIZE];
    size_t text_len = (size_t)(text_end - text_start);
    if (text_len >= sizeof(text)) {
        text_len = sizeof(text) - 1;
    }
    for (size_t i = 0; i < text_len; ++i) {
        text[i] = text_start[i];
    }

    int append = 0;
    const char* after = redir + 1;
    if (*after == '>') {
        append = 1;
        ++after;
    }

    /* Skip spaces to filename */
    while (*after == ' ') {
        ++after;
    }
    if (*after == '\0') {
        console_write_line("efile: missing file name");
        return;
    }

    const char* fname_start = after;
    const char* fname_end = fname_start;
    while (*fname_end && *fname_end != ' ') {
        ++fname_end;
    }

    char name[ENIXNEL_MAX_NAME_LEN + 1];
    size_t name_len = (size_t)(fname_end - fname_start);
    if (name_len >= sizeof(name)) {
        name_len = sizeof(name) - 1;
    }
    for (size_t i = 0; i < name_len; ++i) {
        name[i] = fname_start[i];
    }
    name[name_len] = '\0';

    char full[ENIXNEL_MAX_NAME_LEN + 1];
    path_join(current_dir, name, full, sizeof(full));

    if (fs_write_file(full, text, text_len, append) != 0) {
        console_write("efile: failed to write ");
        console_write_line(name);
    }
}

/* Change directory: cd <name> or cd .. */
static void cli_cmd_cd(const char* args)
{
    char name[ENIXNEL_MAX_NAME_LEN + 1];
    char target[ENIXNEL_MAX_NAME_LEN + 1];

    cli_first_arg(args, name, sizeof(name));
    if (name[0] == '\0') {
        console_write_line("cd: missing name");
        return;
    }

    if (k_streq(name, ".")) {
        /* Stay in current directory */
        return;
    }

    if (k_streq(name, "..")) {
        /* Go to parent of current_dir */
        char parent[ENIXNEL_MAX_NAME_LEN + 1];
        path_parent(current_dir, parent, sizeof(parent));
        /* parent may be "" (root) */
        /* Copy back to current_dir */
        size_t i = 0;
        while (parent[i] && i < sizeof(current_dir) - 1) {
            current_dir[i] = parent[i];
            ++i;
        }
        current_dir[i] = '\0';
        return;
    }

    /* Normal cd: join current_dir and name, and require that it's a directory */
    path_join(current_dir, name, target, sizeof(target));

    int idx = fs_find_index(target);
    if (idx < 0 || !fs_entries[idx].used || !fs_entries[idx].is_dir) {
        console_write("cd: no such directory: ");
        console_write_line(name);
        return;
    }

    /* Set new current_dir */
    size_t i = 0;
    while (target[i] && i < sizeof(current_dir) - 1) {
        current_dir[i] = target[i];
        ++i;
    }
    current_dir[i] = '\0';
}

static void cli_handle_line(const char* line)
{
    char cmd[16];
    const char* args = 0;

    cli_split_command(line, cmd, sizeof(cmd), &args);

    if (cmd[0] == '\0') {
        return; /* empty line */
    }

    if (k_streq(cmd, "help")) {
        cli_cmd_help();
    } else if (k_streq(cmd, "echo")) {
        cli_cmd_echo(args);
    } else if (k_streq(cmd, "crtdir")) {
        cli_cmd_crtdir(args);
    } else if (k_streq(cmd, "cfile")) {
        cli_cmd_cfile(args);
    } else if (k_streq(cmd, "deldir")) {
        cli_cmd_deldir(args);
    } else if (k_streq(cmd, "dfile")) {
        cli_cmd_dfile(args);
    } else if (k_streq(cmd, "sdir")) {
        cli_cmd_sdir();
    } else if (k_streq(cmd, "sfile")) {
        cli_cmd_sfile(args);
    } else if (k_streq(cmd, "efile")) {
        cli_cmd_efile(args);
    } else if (k_streq(cmd, "clr")) {
        cli_cmd_clr();
    } else if (k_streq(cmd, "cd")) {
        cli_cmd_cd(args);
    } else {
        console_write("Unknown command: ");
        console_write_line(cmd);
    }
}

static void cli_print_prompt(void)
{
    /* Show a simple PWD-style prefix in the prompt */
    if (current_dir[0] == '\0') {
        /* Root */
        console_write("/$ ");
    } else {
        console_putc('/');
        console_write(current_dir);
        console_write("$ ");
    }
}

static void cli_loop(void)
{
    char line[128];

    for (;;) {
        cli_print_prompt();
        console_read_line(line, sizeof(line));
        cli_handle_line(line);
    }
}

void kernel_main(void)
{
    console_clear();
    console_write_line("Welcome to Enixnel");
    console_write_line("-------------------");
    console_write_line("");
    console_write_line("Type 'help' for a list of commands.");
    console_write_line("");

    /* Initialize basic filesystem layout: /bin and /user */
    fs_init_layout();

    /* Start the user in /user by default */
    current_dir[0] = '\0';
    path_join("", "user", current_dir, sizeof(current_dir));

    cli_loop();
}