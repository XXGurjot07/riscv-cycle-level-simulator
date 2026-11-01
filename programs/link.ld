ENTRY(_start)

MEMORY{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64K
}

_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS{
    . = ORIGIN(RAM);
    .text   : { *(.text.init) *(.text*) } > RAM
    .rodata : { *(.rodata*) } > RAM
    .data   : { *(.data*) } > RAM
    .tohost : { *(.tohost) } > RAM
    .bss    : { *(.bss*) *(COMMON) } > RAM
}