/*
 * Enixnel Multiboot header (Multiboot v1)
 */

    .section .multiboot
    .align 4

    /* Magic number */
    .long   0x1BADB002

    /* Flags:
     *  bit 0: align modules on page boundaries
     *  bit 1: request memory information
     */
    .long   0x00000003

    /* Checksum: -(magic + flags) */
    .long   -(0x1BADB002 + 0x00000003)