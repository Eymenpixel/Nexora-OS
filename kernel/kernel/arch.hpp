#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern "C" void gdt_flush(const void* pointer);
extern "C" void idt_flush(const void* pointer);
extern "C" void irq0_stub();
extern "C" void irq1_stub();
extern "C" void unknown_interrupt_stub();
extern "C" void syscall_stub();
extern "C" void enter_user_mode(u32 entry, u32 stack);
extern "C" void exception_0(); extern "C" void exception_1();
extern "C" void exception_2(); extern "C" void exception_3();
extern "C" void exception_4(); extern "C" void exception_5();
extern "C" void exception_6(); extern "C" void exception_7();
extern "C" void exception_8(); extern "C" void exception_9();
extern "C" void exception_10(); extern "C" void exception_11();
extern "C" void exception_12(); extern "C" void exception_13();
extern "C" void exception_14(); extern "C" void exception_15();
extern "C" void exception_16(); extern "C" void exception_17();
extern "C" void exception_18(); extern "C" void exception_19();
extern "C" void exception_20(); extern "C" void exception_21();
extern "C" void exception_22(); extern "C" void exception_23();
extern "C" void exception_24(); extern "C" void exception_25();
extern "C" void exception_26(); extern "C" void exception_27();
extern "C" void exception_28(); extern "C" void exception_29();
extern "C" void exception_30(); extern "C" void exception_31();

void arch_init();
void interrupts_enable();
void interrupts_disable();
void pic_init();
void pit_init(u32 frequency);
u32 timer_ticks();
void keyboard_init();
bool keyboard_pop(char* out);
