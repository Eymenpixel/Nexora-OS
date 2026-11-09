#pragma once

typedef unsigned int u32;

enum GetallowCaller : u32 { GETALLOW_CALLER_KERNEL_CONSOLE = 1, GETALLOW_CALLER_USER_DEMO = 2 };
enum GetallowDecision : u32 { GETALLOW_ALLOWED = 0, GETALLOW_DENIED = 1, GETALLOW_UNKNOWN = 2 };

struct GetallowAuditRecord {
    char capability[32];
    u32 caller;
    GetallowDecision decision;
    u32 tick;
};

GetallowDecision getallow_check(GetallowCaller caller, const char* capability, u32 tick);
void getallow_audit_event(GetallowCaller caller, const char* event, GetallowDecision decision, u32 tick);
void getallow_session_set(bool active, u32 expires_at);
bool getallow_session_active(u32 tick);
bool getallow_grant_demo(const char* capability, u32 tick);
bool getallow_revoke_demo(const char* capability, u32 tick);
u32 getallow_audit_count();
bool getallow_audit_read(u32 index, GetallowAuditRecord* record);
