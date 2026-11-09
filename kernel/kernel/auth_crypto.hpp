#pragma once

typedef unsigned char u8;
typedef unsigned int u32;

bool auth_random_bytes(u8* output, u32 length);
void auth_pbkdf2_sha256(const u8* password, u32 password_length, const u8* salt, u32 salt_length, u32 iterations, u8 output[32]);
bool auth_constant_time_equal(const u8* a, const u8* b, u32 length);
