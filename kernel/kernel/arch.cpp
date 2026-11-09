#include "arch.hpp"
#include "getallow_policy.hpp"

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

struct __attribute__((packed)) GdtEntry { u16 limit_low; u16 base_low; u8 base_middle; u8 access; u8 granularity; u8 base_high; };
struct __attribute__((packed)) TablePointer { u16 limit; u32 base; };
struct __attribute__((packed)) IdtEntry { u16 base_low; u16 selector; u8 zero; u8 flags; u16 base_high; };

static GdtEntry gdt[6];
static IdtEntry idt[256];
static TablePointer gdt_pointer;
static TablePointer idt_pointer;
static volatile u32 ticks = 0;
static volatile char key_queue[128];
static volatile u8 key_read = 0;
static volatile u8 key_write = 0;
static bool left_shift = false;
static bool right_shift = false;
static bool caps_lock = false;
static u32 ring0_stack[4096] __attribute__((aligned(16)));
static u32 tss[26] __attribute__((aligned(16)));
extern "C" volatile u32 user_task_faulted;

static void set_gdt(u32 index, u32 base, u32 limit, u8 access, u8 granularity) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

static void set_idt(u8 vector, void (*handler)()) {
    const u32 address = reinterpret_cast<u32>(handler);
    idt[vector].base_low = address & 0xFFFF;
    idt[vector].base_high = (address >> 16) & 0xFFFF;
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].flags = 0x8E;
}

void arch_init() {
    gdt_pointer.limit = sizeof(gdt) - 1;
    gdt_pointer.base = reinterpret_cast<u32>(gdt);
    set_gdt(0, 0, 0, 0, 0);
    set_gdt(1, 0, 0xFFFFF, 0x9A, 0xCF);
    set_gdt(2, 0, 0xFFFFF, 0x92, 0xCF);
    set_gdt(3, 0, 0xFFFFF, 0xFA, 0xCF);
    set_gdt(4, 0, 0xFFFFF, 0xF2, 0xCF);
    tss[1] = reinterpret_cast<u32>(ring0_stack) + sizeof(ring0_stack);
    tss[2] = 0x10;
    reinterpret_cast<u16*>(&tss[25])[1] = sizeof(tss);
    set_gdt(5, reinterpret_cast<u32>(tss), sizeof(tss) - 1, 0x89, 0x00);
    gdt_flush(&gdt_pointer);
    u16 tss_selector = 0x28;
    __asm__ volatile ("ltr %0" : : "r"(tss_selector));

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base = reinterpret_cast<u32>(idt);
    for (u32 i = 0; i < 256; ++i) set_idt(static_cast<u8>(i), unknown_interrupt_stub);
    void (*exceptions[32])() = {
        exception_0, exception_1, exception_2, exception_3, exception_4, exception_5, exception_6, exception_7,
        exception_8, exception_9, exception_10, exception_11, exception_12, exception_13, exception_14, exception_15,
        exception_16, exception_17, exception_18, exception_19, exception_20, exception_21, exception_22, exception_23,
        exception_24, exception_25, exception_26, exception_27, exception_28, exception_29, exception_30, exception_31
    };
    for (u32 i = 0; i < 32; ++i) set_idt(static_cast<u8>(i), exceptions[i]);
    set_idt(32, irq0_stub);
    set_idt(33, irq1_stub);
    set_idt(128, syscall_stub);
    idt[128].flags = 0xEE;
    idt_flush(&idt_pointer);
}

void pic_init() {
    const u8 master_mask = inb(0x21);
    const u8 slave_mask = inb(0xA1);
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, static_cast<u8>((master_mask & 0xFC) | 0xFC));
    outb(0xA1, static_cast<u8>(slave_mask | 0xFF));
}

void pit_init(u32 frequency) {
    if (frequency < 19 || frequency > 1193180) frequency = 100;
    const u32 divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, static_cast<u8>(divisor & 0xFF));
    outb(0x40, static_cast<u8>((divisor >> 8) & 0xFF));
}

u32 timer_ticks() { return ticks; }

void keyboard_init() {
    key_read = 0;
    key_write = 0;
    left_shift = false;
    right_shift = false;
    caps_lock = false;
}

static void queue_key(char value) {
    const u8 next = static_cast<u8>((key_write + 1) % 128);
    if (next == key_read) return;
    key_queue[key_write] = value;
    key_write = next;
}

static void keyboard_irq() {
    const u8 code = inb(0x60);
    if (code == 0x2A) { left_shift = true; return; }
    if (code == 0xAA) { left_shift = false; return; }
    if (code == 0x36) { right_shift = true; return; }
    if (code == 0xB6) { right_shift = false; return; }
    if (code & 0x80) return;
    if (code == 0x3A) { caps_lock = !caps_lock; return; }

    static const char normal[58] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
        'd','f','g','h','j','k','l',';','\'', '`',0,'\\','z','x','c','v',
        'b','n','m',',','.','/',0,'*',0,' '
    };
    static const char shifted[58] = {
        0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
        'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
        'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
        'B','N','M','<','>','?',0,'*',0,' '
    };
    if (code >= sizeof(normal)) return;
    const bool shift = left_shift || right_shift;
    char value = shift ? shifted[code] : normal[code];
    if (caps_lock && value >= 'a' && value <= 'z' && !shift) value = static_cast<char>(value - 'a' + 'A');
    else if (caps_lock && value >= 'A' && value <= 'Z' && shift) value = static_cast<char>(value - 'A' + 'a');
    if (value) queue_key(value);
}

extern "C" void interrupt_dispatch(u32 vector) {
    if (vector == 32) ++ticks;
    else if (vector == 33) keyboard_irq();
    outb(0x20, 0x20);
}

struct ExceptionFrame {
    u32 edi, esi, ebp, saved_esp, ebx, edx, ecx, eax;
    u32 vector, error, eip, cs, eflags, user_esp, user_ss;
};

extern "C" u32 exception_dispatch(u32 vector, ExceptionFrame* frame) {
    if (frame && (frame->cs & 3) == 3) {
        interrupts_disable();
        user_task_faulted = 1;
        getallow_audit_event(GETALLOW_CALLER_USER_DEMO, "process.user-fault", GETALLOW_DENIED, timer_ticks());
        static const char message[] = "Getallow: ring-3 task terminated after CPU exception ";
        for (u32 i = 0; message[i]; ++i) {
            for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, message[i]); break; }
        }
        if (vector < 32) {
            const char* name = vector == 14 ? "page fault" : vector == 13 ? "general protection fault" : "CPU fault";
            for (u32 i = 0; name[i]; ++i) {
                for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, name[i]); break; }
            }
            if (vector == 14) {
                static const char address_prefix[] = " at 0x";
                for (u32 i = 0; address_prefix[i]; ++i) {
                    for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, address_prefix[i]); break; }
                }
                u32 fault_address;
                __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
                static const char hex[] = "0123456789ABCDEF";
                for (int shift = 28; shift >= 0; shift -= 4) {
                    const char digit = hex[(fault_address >> shift) & 0xF];
                    for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, digit); break; }
                }
            }
        }
        outb(0x3F8, '\n');
        return 1;
    }
    interrupts_disable();
    interrupts_disable();
    static const char* names[32] = {
        "divide error", "debug", "NMI", "breakpoint", "overflow", "bound range", "invalid opcode", "device unavailable",
        "double fault", "coprocessor overrun", "invalid TSS", "segment not present", "stack fault", "general protection", "page fault", "reserved",
        "x87 exception", "alignment check", "machine check", "SIMD exception", "virtualization", "control protection", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved", "hypervisor injection", "VMM communication", "security exception", "reserved"
    };
    static const char prefix[] = "BlackBit fatal CPU exception: ";
    for (u32 i = 0; prefix[i]; ++i) {
        for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, prefix[i]); break; }
    }
    const char* name = vector < 32 ? names[vector] : "unexpected interrupt";
    for (u32 i = 0; name[i]; ++i) {
        for (u32 timeout = 0; timeout < 100000; ++timeout) if (inb(0x3FD) & 0x20) { outb(0x3F8, name[i]); break; }
    }
    outb(0x3F8, '\n');
    for (;;) __asm__ volatile ("hlt");
}

bool keyboard_pop(char* out) {
    if (key_read == key_write) return false;
    *out = key_queue[key_read];
    key_read = static_cast<u8>((key_read + 1) % 128);
    return true;
}

void interrupts_enable() { __asm__ volatile ("sti"); }
void interrupts_disable() { __asm__ volatile ("cli"); }
