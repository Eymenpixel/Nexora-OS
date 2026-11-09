.section .text
.code32
.extern interrupt_dispatch
.extern exception_dispatch
.extern syscall_dispatch

.macro IRQ_STUB name, vector
.global \name
\name:
    push $\vector
    jmp irq_common
.endm

IRQ_STUB irq0_stub, 32
IRQ_STUB irq1_stub, 33

.global syscall_stub
syscall_stub:
    pusha
    cld
    push %esp
    call syscall_dispatch
    add $4, %esp
    cmp $1, %eax
    je syscall_return_kernel
    popa
    iret

syscall_return_kernel:
    push 40(%esp)
    popfl
    mov user_kernel_esp, %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

.global enter_user_mode
enter_user_mode:
    mov 4(%esp), %eax
    mov 8(%esp), %edx
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov %esp, user_kernel_esp
    push $0x23
    push %edx
    pushfl
    pop %ecx
    or $0x200, %ecx
    push %ecx
    push $0x1b
    push %eax
    iret

.macro EXC_NOERR name, vector
.global \name
\name:
    push $0
    push $\vector
    jmp exception_common
.endm

.macro EXC_ERR name, vector
.global \name
\name:
    push $\vector
    jmp exception_common
.endm

EXC_NOERR exception_0, 0
EXC_NOERR exception_1, 1
EXC_NOERR exception_2, 2
EXC_NOERR exception_3, 3
EXC_NOERR exception_4, 4
EXC_NOERR exception_5, 5
EXC_NOERR exception_6, 6
EXC_NOERR exception_7, 7
EXC_ERR   exception_8, 8
EXC_NOERR exception_9, 9
EXC_ERR   exception_10, 10
EXC_ERR   exception_11, 11
EXC_ERR   exception_12, 12
EXC_ERR   exception_13, 13
EXC_ERR   exception_14, 14
EXC_NOERR exception_15, 15
EXC_NOERR exception_16, 16
EXC_ERR   exception_17, 17
EXC_NOERR exception_18, 18
EXC_NOERR exception_19, 19
EXC_NOERR exception_20, 20
EXC_ERR   exception_21, 21
EXC_NOERR exception_22, 22
EXC_NOERR exception_23, 23
EXC_NOERR exception_24, 24
EXC_NOERR exception_25, 25
EXC_NOERR exception_26, 26
EXC_NOERR exception_27, 27
EXC_NOERR exception_28, 28
EXC_ERR   exception_29, 29
EXC_ERR   exception_30, 30
EXC_NOERR exception_31, 31

.global unknown_interrupt_stub
unknown_interrupt_stub:
    push $0
    push $255
    jmp exception_common

irq_common:
    pusha
    cld
    push 32(%esp)
    call interrupt_dispatch
    add $4, %esp
    popa
    add $4, %esp
    iret

exception_common:
    pusha
    cld
    mov %esp, %eax
    push %eax
    push 32(%eax)
    call exception_dispatch
    add $8, %esp
    cmp $1, %eax
    je exception_return_kernel
1:
    cli
    hlt
    jmp 1b

exception_return_kernel:
    push 48(%esp)
    popfl
    mov user_kernel_esp, %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

.global gdt_flush
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)
    ljmp $0x08, $gdt_reload_segments
gdt_reload_segments:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ret

.global idt_flush
idt_flush:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

.section .bss
.align 4
.global user_kernel_esp
user_kernel_esp:
    .long 0
