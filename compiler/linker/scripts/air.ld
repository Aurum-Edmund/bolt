/*
 * Air kernel linker script (Stage-0 prototype).
 * TODO: define precise section layout and entry placement.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .text : ALIGN(16)
    {
        *(.text.entry)
        *(.text*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
        *(.bss*)
        *(COMMON)
    }
}
