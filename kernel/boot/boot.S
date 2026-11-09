.section .multiboot2, "a"
.align 8
mb2_header_start:
    .long 0xe85250d6
    .long 0
    .long mb2_header_end - mb2_header_start
    .long -(0xe85250d6 + (mb2_header_end - mb2_header_start))
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .bss
.align 16
boot_stack_bottom:
    .skip 16384
boot_stack_top:

.section .text
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli
    mov $boot_stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
1:
    cli
    hlt
    jmp 1b
