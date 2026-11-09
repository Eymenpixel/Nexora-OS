#pragma once

#include "auth_store.hpp"

enum AuthResult { AUTH_RESULT_OK, AUTH_RESULT_BAD, AUTH_RESULT_LOCKED, AUTH_RESULT_OFFLINE, AUTH_RESULT_CORRUPT, AUTH_RESULT_ALREADY_SET, AUTH_RESULT_RANDOM_UNAVAILABLE, AUTH_RESULT_STORAGE_ERROR };

AuthStoreState auth_init();
AuthStoreState auth_status();
AuthResult auth_enroll(const u8* password, u32 length);
AuthResult auth_login(const u8* password, u32 length, u32 now);
void auth_wipe(void* memory, u32 length);
