ENTRY(_start)

SECTIONS
{
    . = 1M;
    __kernel_start = .;
    .multiboot2 : ALIGN(8) { KEEP(*(.multiboot2)) }
    .text : ALIGN(4K) { *(.text*) }
    .userblob : ALIGN(16) { KEEP(*(.userblob)) }
    .rodata : ALIGN(4K) { *(.rodata*) }
    .data : ALIGN(4K) { *(.data*) }
    .bss : ALIGN(4K) { *(COMMON) *(.bss*) }
    __kernel_end = .;
}
