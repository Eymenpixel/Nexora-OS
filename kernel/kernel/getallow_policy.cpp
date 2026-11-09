#include "getallow_policy.hpp"

static const u32 AUDIT_CAPACITY = 16;
static GetallowAuditRecord audit_log[AUDIT_CAPACITY];
static u32 audit_next = 0;
static u32 audit_count = 0;
static bool admin_session = false;
static u32 session_expires = 0;
static bool demo_memory_inspect = false;

static bool same(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

void getallow_audit_event(GetallowCaller caller, const char* event, GetallowDecision decision, u32 tick) {
    const char* source = event ? event : "(null)";
    u32 i = 0;
    while (source[i] && i < sizeof(audit_log[audit_next].capability) - 1) {
        audit_log[audit_next].capability[i] = source[i];
        ++i;
    }
    audit_log[audit_next].capability[i] = 0;
    audit_log[audit_next].caller = static_cast<u32>(caller);
    audit_log[audit_next].decision = decision;
    audit_log[audit_next].tick = tick;
    audit_next = (audit_next + 1) % AUDIT_CAPACITY;
    if (audit_count < AUDIT_CAPACITY) ++audit_count;
}

static bool safe_console_capability(const char* capability) {
    return same(capability, "system.info") || same(capability, "memory.inspect");
}

bool getallow_session_active(u32 tick) {
    if (!admin_session) return false;
    if (static_cast<int>(tick - session_expires) >= 0) {
        admin_session = false;
        demo_memory_inspect = false;
        return false;
    }
    return true;
}

void getallow_session_set(bool active, u32 expires_at) {
    admin_session = active;
    session_expires = active ? expires_at : 0;
    if (!active) demo_memory_inspect = false;
}

GetallowDecision getallow_check(GetallowCaller caller, const char* capability, u32 tick) {
    GetallowDecision decision = GETALLOW_DENIED;
    if (!capability || !*capability) decision = GETALLOW_UNKNOWN;
    else if (caller == GETALLOW_CALLER_KERNEL_CONSOLE && safe_console_capability(capability)) decision = GETALLOW_ALLOWED;
    else if (caller == GETALLOW_CALLER_KERNEL_CONSOLE && same(capability, "process.demo")) decision = GETALLOW_ALLOWED;
    else if (caller == GETALLOW_CALLER_KERNEL_CONSOLE && getallow_session_active(tick) &&
             (same(capability, "system.halt") || same(capability, "auth.admin"))) decision = GETALLOW_ALLOWED;
    else if (caller == GETALLOW_CALLER_USER_DEMO && same(capability, "system.info")) decision = GETALLOW_ALLOWED;
    else if (caller == GETALLOW_CALLER_USER_DEMO && same(capability, "memory.inspect") &&
             getallow_session_active(tick) && demo_memory_inspect) decision = GETALLOW_ALLOWED;

    const char* source = capability ? capability : "(null)";
    u32 i = 0;
    while (source[i] && i < sizeof(audit_log[audit_next].capability) - 1) {
        audit_log[audit_next].capability[i] = source[i];
        ++i;
    }
    audit_log[audit_next].capability[i] = 0;
    audit_log[audit_next].caller = static_cast<u32>(caller);
    audit_log[audit_next].decision = decision;
    audit_log[audit_next].tick = tick;
    audit_next = (audit_next + 1) % AUDIT_CAPACITY;
    if (audit_count < AUDIT_CAPACITY) ++audit_count;
    return decision;
}

bool getallow_grant_demo(const char* capability, u32 tick) {
    const bool authorized = getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.admin", tick) == GETALLOW_ALLOWED;
    if (!authorized || !same(capability, "memory.inspect")) {
        getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "grant.user-demo.memory.inspect", GETALLOW_DENIED, tick);
        return false;
    }
    demo_memory_inspect = true;
    getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "grant.user-demo.memory.inspect", GETALLOW_ALLOWED, tick);
    return true;
}

bool getallow_revoke_demo(const char* capability, u32 tick) {
    const bool authorized = getallow_check(GETALLOW_CALLER_KERNEL_CONSOLE, "auth.admin", tick) == GETALLOW_ALLOWED;
    if (!authorized || !same(capability, "memory.inspect")) {
        getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "revoke.user-demo.memory.inspect", GETALLOW_DENIED, tick);
        return false;
    }
    demo_memory_inspect = false;
    getallow_audit_event(GETALLOW_CALLER_KERNEL_CONSOLE, "revoke.user-demo.memory.inspect", GETALLOW_ALLOWED, tick);
    return true;
}

u32 getallow_audit_count() { return audit_count; }

bool getallow_audit_read(u32 index, GetallowAuditRecord* record) {
    if (!record || index >= audit_count) return false;
    const u32 oldest = (audit_next + AUDIT_CAPACITY - audit_count) % AUDIT_CAPACITY;
    *record = audit_log[(oldest + index) % AUDIT_CAPACITY];
    return true;
}
