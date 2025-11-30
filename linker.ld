ENTRY(_start)

SECTIONS
{
  . = 1M;

  /* Put Multiboot header at the very start of the .text segment */
  .text :
  {
    *(.multiboot)
    *(.text*)
  }

  .rodata :
  {
    *(.rodata*)
  }

  .data :
  {
    *(.data*)
  }

  .bss :
  {
    *(.bss*)
    *(COMMON)
  }
}