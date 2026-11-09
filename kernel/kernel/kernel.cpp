#include "arch.hpp"
#include "memory.hpp"
#include "paging.hpp"
#include "getallow_policy.hpp"
#include "auth.hpp"
#include "auth_crypto.hpp"

static volatile u16* const VGA = reinterpret_cast<volatile u16*>(0xB8000);
static u32 row = 0;
static u32 col = 0;
static u8 color = 0x0F;

static void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void scroll() {
    if (row < 25) return;
    for (u32 y = 1; y < 25; ++y)
        for (u32 x = 0; x < 80; ++x)
            VGA[(y - 1) * 80 + x] = VGA[y * 80 + x];
    for (u32 x = 0; x < 80; ++x) VGA[24 * 80 + x] = (u16(color) << 8) | ' ';
    row = 24;
}

static void put(char c) {
    if (c == '\n') { col = 0; ++row; scroll(); return; }
    if (c == '\r') { col = 0; return; }
    VGA[row * 80 + col] = (u16(color) << 8) | static_cast<u8>(c);
    if (++col >= 80) { col = 0; ++row; scroll(); }
}

static void print(const char* text) { while (*text) put(*text++); }
static void serial_put(char c);
static void print_u32(u32 value) {
    char digits[10];
    u32 count = 0;
    do { digits[count++] = static_cast<char>('0' + value % 10); value /= 10; } while (value);
    while (count) { const char digit = digits[--count]; put(digit); serial_put(digit); }
}
static void clear() {
    for (u32 i = 0; i < 80 * 25; ++i) VGA[i] = (u16(color) << 8) | ' ';
    row = 0; col = 0;
}

static void serial_init() {
    outb(0x3F9, 0x00); outb(0x3FB, 0x80);
    outb(0x3F8, 0x03); outb(0x3F9, 0x00);
    outb(0x3FB, 0x03); outb(0x3FA, 0xC7); outb(0x3FC, 0x0B);
}

static void serial_put(char c) {
    for (u32 timeout = 0; timeout < 1000000; ++timeout)
        if (inb(0x3FD) & 0x20) { outb(0x3F8, static_cast<u8>(c)); return; }
}

static void serial_print(const char* text) { while (*text) serial_put(*text++); }
static void write(const char* text) { print(text); serial_print(text); }
static void line(const char* text) { write(text); put('\n'); serial_put('\n'); }

static bool equal(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static char input[80];
static u32 input_len = 0;
enum AuthFlow { AUTH_FLOW_NONE, AUTH_FLOW_SETUP_PASSWORD, AUTH_FLOW_SETUP_CONFIRM, AUTH_FLOW_LOGIN };
static AuthFlow auth_flow = AUTH_FLOW_NONE;
static u8 secret_input[64];
static u32 secret_length = 0;
static bool secret_overflow = false;
static u8 pending_secret[64];
static u32 pending_secret_length = 0;

extern "C" const u8 user_demo_start[];
extern "C" const u8 user_demo_end[];
extern "C" const u8 user_fault_start[];
extern "C" const u8 user_fault_end[];
extern "C" { volatile u32 user_task_faulted = 0; }

struct SyscallFrame { u32 edi, esi, ebp, saved_esp, ebx, edx, ecx, eax; };

extern "C" u32 syscall_dispatch(SyscallFrame* frame) {
    if (frame->eax == 0) return 1;
    if (frame->eax == 1) {
        const u32 address = frame->ebx;
        if (address < 0x40000000 || address >= 0x40001000) { frame->eax = 1; return 0; }
        char text[128];
        u32 i = 0;
        while (i < sizeof(text) - 1 && address + i < 0x40001000) {
            text[i] = *reinterpret_cast<const char*>(address + i);
            if (!text[i]) break;
            ++i;
        }
        text[i] = 0;
        write(text);
        frame->eax = 0;
        return 0;
    }
    if (frame->eax == 2) {
        const char* capability = 0;
        if (frame->ebx == 1) capability = "system.info";
        else if (frame->ebx == 2) capability = "memory.inspect";
        else if (frame->ebx == 3) capability = "system.halt";
        const GetallowDecision decision = getallow_check(GETALLOW_CALLER_USER_DEMO, capability, timer_ticks());
        frame->eax = decision == GETALLOW_ALLOWED ? 0 : 1;
        return 0;
    }
    frame->eax = 0xFFFFFFFF;
    return 0;
}

static bool starts_with(const char* value, const char* prefix) {
    while (*prefix) { if (*value++ != *prefix++) return false; }
    return true;
}

static void finish_auth_flow() {
    auth_flow = AUTH_FLOW_NONE;
    secret_length = 0;
    input_len = 0;
    write("blackbit> ");
}

static void begin_auth_flow(AuthFlow flow) {
    auth_flow = flow;
    secret_length = 0;
    secret_overflow = false;
    auth_wipe(secret_input, sizeof(secret_input));
    if (flow == AUTH_FLOW_SETUP_PASSWORD) write("New admin password (12-63 chars): ");
    else if (flow == AUTH_FLOW_SETUP_CONFIRM) write("Confirm admin password: ");
    else write("Admin password: ");
}

static void complete_setup() {
    if (secret_length < 12) {
        auth_wipe(secret_input, sizeof(secret_input));
        line("Password must contain at least 12 characters.");
        finish_auth_flow();
        return;
    }
    pending_secret_length = secret_length;
    for (u32 i = 0; i < secret_length; ++i) pending_secret[i] = secret_input[i];
    auth_wipe(secret_input, sizeof(secret_input));
    secret_length = 0;
    auth_flow = AUTH_FLOW_SETUP_CONFIRM;
    write("Confirm admin password: ");
}

static void accept_secret(char c) {
    if (c == '\r') c = '\n';
    if (c == '\b' || c == 127) {
        if (secret_length) { --secret_length; secret_input[secret_length] = 0; put('\b'); put(' '); put('\b'); }
        return;
    }
    if (c != '\n') {
        if (secret_length < sizeof(secret_input) - 1) { secret_input[secret_length++] = static_cast<u8>(c); put('*'); }
        else secret_overflow = true;
        return;
    }
    put('\n');
    serial_put('\n');
    if (secret_overflow) {
        auth_wipe(secret_input, sizeof(secret_input));
        auth_wipe(pending_secret, sizeof(pending_secret));
        pending_secret_length = 0;
        line("Password exceeds the 63-character limit; no change was made.");
        finish_auth_flow();
        return;
    }
    if (auth_flow == AUTH_FLOW_SETUP_PASSWORD) { complete_setup(); return; }
    if (auth_flow == AUTH_FLOW_SETUP_CONFIRM) {
        const bool matches = secret_length == pending_secret_length &&
            auth_constant_time_equal(secret_input, pending_secret, pending_secret_length);
        if (!matches) {
            auth_wipe(secret_input, sizeof(secret_input));
            auth_wipe(pending_secret, sizeof(pending_secret));
            pending_secret_length = 0;
            getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.enroll", GETALLOW_DENIED, timer_ticks());
            line("Passwords did not match; no credential was stored.");
            finish_auth_flow();
            return;
        }
        const AuthResult result = auth_enroll(secret_input, secret_length);
        auth_wipe(secret_input, sizeof(secret_input));
        auth_wipe(pending_secret, sizeof(pending_secret));
        pending_secret_length = 0;
        if (result == AUTH_RESULT_OK) {
            getallow_session_set(true, timer_ticks() + 30000);
            getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.enroll", GETALLOW_ALLOWED, timer_ticks());
            line("Admin credential stored. Getallow session active for five minutes.");
        } else {
            getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.enroll", GETALLOW_DENIED, timer_ticks());
            if (result == AUTH_RESULT_RANDOM_UNAVAILABLE) line("Enrollment refused: CPU cryptographic random source unavailable.");
            else if (result == AUTH_RESULT_STORAGE_ERROR) line("Enrollment failed while writing the credential store.");
            else if (result == AUTH_RESULT_OFFLINE) line("Enrollment failed: persistent IDE credential disk is offline.");
            else line("Enrollment failed; no credential was accepted.");
        }
        finish_auth_flow();
        return;
    }
    const AuthResult result = auth_login(secret_input, secret_length, timer_ticks());
    auth_wipe(secret_input, sizeof(secret_input));
    if (result == AUTH_RESULT_OK) {
        getallow_session_set(true, timer_ticks() + 30000);
        getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.login", GETALLOW_ALLOWED, timer_ticks());
        line("Authentication succeeded. Getallow admin session active for five minutes.");
    } else if (result == AUTH_RESULT_LOCKED) { getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.login", GETALLOW_DENIED, timer_ticks()); line("Authentication failed or rate-limited; wait 30 seconds before retrying."); }
    else { getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.login", GETALLOW_DENIED, timer_ticks()); line("Authentication failed."); }
    finish_auth_flow();
}

static void getallow_command(const char* argument) {
    if (!*argument || equal(argument, "status")) {
        line("Getallow: deny-by-default; persistent admin credential store:");
        if (auth_status() == AUTH_STORE_READY) line("  configured (PBKDF2-SHA-256 verifier; no plaintext password stored).");
        else if (auth_status() == AUTH_STORE_EMPTY) line("  empty; run getallow setup to enroll the first admin password.");
        else if (auth_status() == AUTH_STORE_CORRUPT) line("  corrupt; locked fail-closed. Do not erase it automatically.");
        else line("  offline; attach the IDE credential disk.");
        line(getallow_session_active(timer_ticks()) ? "Admin session: active (expires within five minutes)." : "Admin session: locked.");
        return;
    }
    if (equal(argument, "list")) {
        line("Kernel console: system.info, memory.inspect, process.demo");
        line("Admin session: system.halt and grant/revoke for user-demo memory.inspect");
        line("Built-in ring-3 demo: system.info; memory.inspect requires an admin grant");
        return;
    }
    if (equal(argument, "setup")) {
        if (auth_status() == AUTH_STORE_EMPTY) begin_auth_flow(AUTH_FLOW_SETUP_PASSWORD);
        else if (auth_status() == AUTH_STORE_READY) line("Admin credential is already configured.");
        else if (auth_status() == AUTH_STORE_CORRUPT) line("Credential store is corrupt; setup is disabled to prevent accidental reset.");
        else line("Credential disk unavailable. Attach the persistent IDE disk and reboot.");
        return;
    }
    if (equal(argument, "login")) {
        if (auth_status() == AUTH_STORE_READY) begin_auth_flow(AUTH_FLOW_LOGIN);
        else if (auth_status() == AUTH_STORE_EMPTY) line("No admin credential yet; run getallow setup.");
        else if (auth_status() == AUTH_STORE_CORRUPT) line("Credential store is corrupt; login is locked.");
        else line("Credential disk unavailable; login is locked.");
        return;
    }
    if (equal(argument, "logout")) {
        getallow_session_set(false, 0);
        getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.logout", GETALLOW_ALLOWED, timer_ticks());
        line("Admin session closed; temporary grants revoked.");
        return;
    }
    if (equal(argument, "grant user-demo memory.inspect")) {
        if (getallow_grant_demo("memory.inspect", timer_ticks())) line("Getallow granted user-demo: memory.inspect (current admin session only).");
        else line("Getallow denied grant; authenticate with getallow login first.");
        return;
    }
    if (equal(argument, "revoke user-demo memory.inspect")) {
        if (getallow_revoke_demo("memory.inspect", timer_ticks())) line("Getallow revoked user-demo: memory.inspect.");
        else line("Getallow denied revoke; authenticate with getallow login first.");
        return;
    }
    if (equal(argument, "audit")) {
        const u32 count = getallow_audit_count();
        if (!count) { line("Getallow audit log is empty."); return; }
        for (u32 i = 0; i < count; ++i) {
            GetallowAuditRecord record;
            if (!getallow_audit_read(i, &record)) continue;
            write(record.decision == GETALLOW_ALLOWED ? "ALLOW " : "DENY  ");
            write(record.caller == GETALLOW_CALLER_USER_DEMO ? "user-demo " : "kernel-console ");
            write(record.capability);
            write(" tick="); print_u32(record.tick); put('\n'); serial_put('\n');
        }
        return;
    }
    if (starts_with(argument, "request ")) {
        const char* capability = argument + 8;
        if (!*capability) { line("Kullanim: getallow request <capability>"); return; }
        const GetallowDecision decision = getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, capability, timer_ticks());
        if (decision == GETALLOW_ALLOWED) line("Getallow: izin verildi.");
        else line("Getallow: REDDEDILDI (kimlik dogrulanmis oturum veya tanimli izin yok).");
        return;
    }
    line("Kullanim: getallow [status|setup|login|logout|list|grant|revoke|audit|request]");
}

static void launch_user_image(const u8* image, const u8* image_end, const char* description) {
    if (getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, "process.demo", timer_ticks()) != GETALLOW_ALLOWED) {
        line("Getallow: ring-3 program launch denied.");
        return;
    }
    void* code_page = page_alloc();
    void* stack_page = page_alloc();
    if (!code_page || !stack_page) {
        if (code_page) page_free(code_page);
        if (stack_page) page_free(stack_page);
        line("Ring-3 process: physical page allocation failed.");
        return;
    }
    const u32 image_size = static_cast<u32>(image_end - image);
    if (image_size > 4096) {
        page_free(code_page); page_free(stack_page);
        line("Ring-3 process image exceeds its 4 KiB code page.");
        return;
    }
    u8* destination = reinterpret_cast<u8*>(code_page);
    for (u32 i = 0; i < image_size; ++i) destination[i] = image[i];
    u8* stack = reinterpret_cast<u8*>(stack_page);
    for (u32 i = 0; i < 4096; ++i) stack[i] = 0;
    const bool code_mapped = paging_map_user(0x40000000, reinterpret_cast<u32>(code_page));
    const bool stack_mapped = paging_map_user(0x40001000, reinterpret_cast<u32>(stack_page));
    if (!code_mapped || !stack_mapped) {
        paging_unmap_user(0x40000000); paging_unmap_user(0x40001000);
        page_free(code_page); page_free(stack_page);
        line("Ring-3 virtual memory mapping failed.");
        return;
    }
    user_task_faulted = 0;
    write("Getallow allowed built-in ring-3 task: "); line(description);
    enter_user_mode(0x40000000, 0x40002000 - 16);
    paging_unmap_user(0x40000000); paging_unmap_user(0x40001000);
    page_free(code_page); page_free(stack_page);
    line(user_task_faulted ? "Kernel contained the ring-3 fault and reclaimed its pages." : "Ring-3 task exited through the syscall gate.");
}

static void prompt() { write("blackbit> "); }

static void run_command() {
    input[input_len] = 0;
    serial_print("\n");
    if (equal(input, "help")) {
        line("Komutlar: help, about, clear, mem, alloc, uptime, userdemo, userfault, getallow, ember status, halt");
    } else if (equal(input, "about")) {
        line("BlackBit: GRUB Multiboot2 x86 kernel prototipi.");
    } else if (starts_with(input, "getallow") && (input[8] == 0 || input[8] == ' ')) {
        getallow_command(input[8] ? input + 9 : "");
    } else if (equal(input, "ember status")) {
        line("EMBER tarayicisi kernel icinde calismiyor (host servisi bagli degil).");
    } else if (equal(input, "uptime")) {
        print("Kernel uptime: "); print_u32(timer_ticks() / 100); line(" seconds.");
    } else if (equal(input, "mem")) {
        if (getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, "memory.inspect", timer_ticks()) != GETALLOW_ALLOWED) {
            line("Getallow: bellek bilgisi reddedildi.");
        } else {
            print("Free physical pages: "); print_u32(memory_free_pages()); line(" (4 KiB each).");
        }
    } else if (equal(input, "alloc")) {
        void* page = page_alloc();
        if (!page) line("Fiziksel bellek tahsisi basarisiz.");
        else { line("4 KiB fiziksel sayfa tahsis edildi; testi tamamlayip serbest birakiliyor."); page_free(page); }
    } else if (equal(input, "userdemo")) {
        launch_user_image(user_demo_start, user_demo_end, "policy demonstration");
    } else if (equal(input, "userfault")) {
        launch_user_image(user_fault_start, user_fault_end, "kernel memory isolation test");
    } else if (equal(input, "clear")) {
        clear();
    } else if (equal(input, "halt")) {
        const GetallowDecision decision = getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, "system.halt", timer_ticks());
        if (decision != GETALLOW_ALLOWED) line("Getallow: sistem durdurma istegi reddedildi.");
        else { interrupts_disable(); for (;;) __asm__ volatile ("hlt"); }
    } else if (input_len != 0) {
        line("Bilinmeyen komut. Komutlar icin help yaz.");
    }
    input_len = 0;
    if (auth_flow == AUTH_FLOW_NONE) prompt();
}

static void accept_character(char c) {
    if (c == '\r') c = '\n';
    if (auth_flow != AUTH_FLOW_NONE) { accept_secret(c); return; }
    if (c == '\n') { run_command(); return; }
    if (c == '\b' || c == 127) {
        if (input_len) { --input_len; put('\b'); put(' '); put('\b'); serial_put('\b'); serial_put(' '); serial_put('\b'); }
        return;
    }
    if (input_len < sizeof(input) - 1) {
        input[input_len++] = c;
        put(c); serial_put(c);
    }
}

extern "C" void kernel_main(u32 magic, u32 multiboot_info) {
    serial_init();
    if (magic != 0x36d76289) {
        line("Boot error: GRUB did not provide a valid Multiboot2 handoff.");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    arch_init();
    paging_init();
    memory_init(multiboot_info);
    pic_init();
    keyboard_init();
    pit_init(100);
    const AuthStoreState auth_state = auth_init();
    interrupts_enable();
    clear();
    line("BlackBit");
    line("BlackBit x86 kernel tabani");
    line("x86 Multiboot2 | IRQ keyboard | PIT 100 Hz | physical page allocator");
    line("Baslatildi. Yardim icin help yaz.");
    if (auth_state == AUTH_STORE_READY) line("Persistent Getallow credential found.");
    else if (auth_state == AUTH_STORE_EMPTY) line("No admin password set. Run getallow setup.");
    else if (auth_state == AUTH_STORE_CORRUPT) line("Credential store corrupt: privileged access stays locked.");
    else line("Credential disk offline: privileged access stays locked.");
    prompt();
    for (;;) {
        if (inb(0x3FD) & 1) {
            accept_character(static_cast<char>(inb(0x3F8)));
            continue;
        }
        char key;
        if (keyboard_pop(&key)) { accept_character(key); continue; }
        __asm__ volatile ("hlt");
    }
}

