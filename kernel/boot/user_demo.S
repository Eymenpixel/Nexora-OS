.section .userblob, "a"
.code32
.set USER_BASE, 0x40000000
.global user_demo_start
.global user_demo_end
user_demo_start:
    mov $1, %eax
    mov $(USER_BASE + user_demo_message - user_demo_start), %ebx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80
    test %eax, %eax
    jnz 3f
    mov $1, %eax
    mov $(USER_BASE + user_demo_info_allowed - user_demo_start), %ebx
    int $0x80
    jmp 4f
3:
    mov $1, %eax
    mov $(USER_BASE + user_demo_info_denied - user_demo_start), %ebx
    int $0x80
4:
    mov $2, %eax
    mov $2, %ebx
    int $0x80
    test %eax, %eax
    jnz 1f

    mov $1, %eax
    mov $(USER_BASE + user_demo_allowed - user_demo_start), %ebx
    int $0x80
    jmp 2f
1:
    mov $1, %eax
    mov $(USER_BASE + user_demo_denied - user_demo_start), %ebx
    int $0x80
2:
    mov $0, %eax
    int $0x80

user_demo_message: .asciz "Ring 3 app: checking system.info and memory.inspect via syscall.\n"
user_demo_info_allowed: .asciz "Getallow granted system.info.\n"
user_demo_info_denied: .asciz "Getallow denied system.info.\n"
user_demo_allowed: .asciz "Getallow granted memory.inspect.\n"
user_demo_denied: .asciz "Getallow denied memory.inspect; kernel kept control.\n"
user_demo_end:

.align 16
.global user_fault_start
.global user_fault_end
user_fault_start:
    mov $0x00100000, %eax
    mov (%eax), %ebx
    mov $0, %eax
    int $0x80
user_fault_end:
