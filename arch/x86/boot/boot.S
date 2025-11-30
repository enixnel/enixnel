/*
 * Enixnel boot entry
 */

    .section .text
    .global _start

_start:
    # Set up stack
    mov $stack_top, %esp

    # Call C kernel entry
    call kernel_main

.halt:
    hlt
    jmp .halt

    .section .bss
    .align 16
stack_bottom:
    .skip 16384    # 16 KiB stack
stack_top: