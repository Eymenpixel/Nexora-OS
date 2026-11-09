#pragma once

#include "auth_crypto.hpp"

enum AuthStoreState { AUTH_STORE_OFFLINE, AUTH_STORE_EMPTY, AUTH_STORE_READY, AUTH_STORE_CORRUPT };

struct AuthRecord {
    char magic[8];
    u32 version;
    u32 generation;
    u32 iterations;
    u8 salt[16];
    u8 verifier[32];
    u32 checksum;
} __attribute__((packed));

AuthStoreState auth_store_load(AuthRecord* record);
bool auth_store_create(const AuthRecord* record);
