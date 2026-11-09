#include "auth.hpp"

static const u32 PBKDF2_ROUNDS = 600000;
static AuthRecord saved_record;
static AuthStoreState store_state = AUTH_STORE_OFFLINE;
static u32 failed_attempts = 0;
static u32 blocked_until = 0;

void auth_wipe(void* memory, u32 length) {
    volatile u8* bytes = reinterpret_cast<volatile u8*>(memory);
    while (length--) *bytes++ = 0;
}

AuthStoreState auth_init() { store_state = auth_store_load(&saved_record); return store_state; }
AuthStoreState auth_status() { return store_state; }

AuthResult auth_enroll(const u8* password, u32 length) {
    if (store_state == AUTH_STORE_OFFLINE) return AUTH_RESULT_OFFLINE;
    if (store_state == AUTH_STORE_CORRUPT) return AUTH_RESULT_CORRUPT;
    if (store_state != AUTH_STORE_EMPTY) return AUTH_RESULT_ALREADY_SET;
    if (!password || length < 12 || length > 63) return AUTH_RESULT_BAD;
    AuthRecord record = {};
    record.iterations = PBKDF2_ROUNDS;
    if (!auth_random_bytes(record.salt, sizeof(record.salt))) { auth_wipe(&record, sizeof(record)); return AUTH_RESULT_RANDOM_UNAVAILABLE; }
    auth_pbkdf2_sha256(password, length, record.salt, sizeof(record.salt), record.iterations, record.verifier);
    if (!auth_store_create(&record)) { auth_wipe(&record, sizeof(record)); return AUTH_RESULT_STORAGE_ERROR; }
    store_state = auth_store_load(&saved_record);
    auth_wipe(&record, sizeof(record));
    return store_state == AUTH_STORE_READY ? AUTH_RESULT_OK : AUTH_RESULT_STORAGE_ERROR;
}

AuthResult auth_login(const u8* password, u32 length, u32 now) {
    if (store_state == AUTH_STORE_OFFLINE) return AUTH_RESULT_OFFLINE;
    if (store_state == AUTH_STORE_CORRUPT) return AUTH_RESULT_CORRUPT;
    if (store_state != AUTH_STORE_READY) return AUTH_RESULT_BAD;
    if (static_cast<int>(now - blocked_until) < 0) return AUTH_RESULT_LOCKED;
    if (!password || !length || length > 63) return AUTH_RESULT_BAD;
    u8 calculated[32];
    auth_pbkdf2_sha256(password, length, saved_record.salt, sizeof(saved_record.salt), saved_record.iterations, calculated);
    const bool match = auth_constant_time_equal(calculated, saved_record.verifier, sizeof(calculated));
    auth_wipe(calculated, sizeof(calculated));
    if (match) { failed_attempts = 0; return AUTH_RESULT_OK; }
    if (++failed_attempts >= 5) { failed_attempts = 0; blocked_until = now + 3000; return AUTH_RESULT_LOCKED; }
    return AUTH_RESULT_BAD;
}
