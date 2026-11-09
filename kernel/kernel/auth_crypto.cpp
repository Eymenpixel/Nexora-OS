#include "auth_crypto.hpp"

typedef unsigned long long u64;

static const u32 K[64] = {
    0x428A2F98,0x71374491,0xB5C0FBCF,0xE9B5DBA5,0x3956C25B,0x59F111F1,0x923F82A4,0xAB1C5ED5,
    0xD807AA98,0x12835B01,0x243185BE,0x550C7DC3,0x72BE5D74,0x80DEB1FE,0x9BDC06A7,0xC19BF174,
    0xE49B69C1,0xEFBE4786,0x0FC19DC6,0x240CA1CC,0x2DE92C6F,0x4A7484AA,0x5CB0A9DC,0x76F988DA,
    0x983E5152,0xA831C66D,0xB00327C8,0xBF597FC7,0xC6E00BF3,0xD5A79147,0x06CA6351,0x14292967,
    0x27B70A85,0x2E1B2138,0x4D2C6DFC,0x53380D13,0x650A7354,0x766A0ABB,0x81C2C92E,0x92722C85,
    0xA2BFE8A1,0xA81A664B,0xC24B8B70,0xC76C51A3,0xD192E819,0xD6990624,0xF40E3585,0x106AA070,
    0x19A4C116,0x1E376C08,0x2748774C,0x34B0BCB5,0x391C0CB3,0x4ED8AA4A,0x5B9CCA4F,0x682E6FF3,
    0x748F82EE,0x78A5636F,0x84C87814,0x8CC70208,0x90BEFFFA,0xA4506CEB,0xBEF9A3F7,0xC67178F2
};

struct Sha256 {
    u32 state[8];
    u64 bits;
    u8 block[64];
    u32 used;
};

static u32 rotate_right(u32 value, u32 count) { return (value >> count) | (value << (32 - count)); }

static void transform(Sha256* ctx, const u8 block[64]) {
    u32 w[64];
    for (u32 i = 0; i < 16; ++i)
        w[i] = (u32(block[i*4]) << 24) | (u32(block[i*4+1]) << 16) | (u32(block[i*4+2]) << 8) | u32(block[i*4+3]);
    for (u32 i = 16; i < 64; ++i) {
        const u32 s0 = rotate_right(w[i-15],7) ^ rotate_right(w[i-15],18) ^ (w[i-15] >> 3);
        const u32 s1 = rotate_right(w[i-2],17) ^ rotate_right(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    u32 a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    u32 e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];
    for (u32 i = 0; i < 64; ++i) {
        const u32 s1 = rotate_right(e,6) ^ rotate_right(e,11) ^ rotate_right(e,25);
        const u32 ch = (e & f) ^ (~e & g);
        const u32 temp1 = h + s1 + ch + K[i] + w[i];
        const u32 s0 = rotate_right(a,2) ^ rotate_right(a,13) ^ rotate_right(a,22);
        const u32 maj = (a & b) ^ (a & c) ^ (b & c);
        const u32 temp2 = s0 + maj;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha_init(Sha256* ctx) {
    const u32 init[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    for (u32 i = 0; i < 8; ++i) ctx->state[i] = init[i];
    ctx->bits = 0; ctx->used = 0;
}

static void sha_update(Sha256* ctx, const u8* data, u32 length) {
    ctx->bits += u64(length) * 8;
    while (length) {
        const u32 take = length < (64 - ctx->used) ? length : (64 - ctx->used);
        for (u32 i = 0; i < take; ++i) ctx->block[ctx->used + i] = data[i];
        ctx->used += take; data += take; length -= take;
        if (ctx->used == 64) { transform(ctx, ctx->block); ctx->used = 0; }
    }
}

static void sha_final(Sha256* ctx, u8 digest[32]) {
    const u64 original_bits = ctx->bits;
    ctx->block[ctx->used++] = 0x80;
    if (ctx->used > 56) {
        while (ctx->used < 64) ctx->block[ctx->used++] = 0;
        transform(ctx, ctx->block); ctx->used = 0;
    }
    while (ctx->used < 56) ctx->block[ctx->used++] = 0;
    for (u32 i = 0; i < 8; ++i) ctx->block[63-i] = static_cast<u8>(original_bits >> (i*8));
    transform(ctx, ctx->block);
    for (u32 i = 0; i < 8; ++i) {
        digest[i*4] = static_cast<u8>(ctx->state[i] >> 24);
        digest[i*4+1] = static_cast<u8>(ctx->state[i] >> 16);
        digest[i*4+2] = static_cast<u8>(ctx->state[i] >> 8);
        digest[i*4+3] = static_cast<u8>(ctx->state[i]);
    }
}

static void hmac_sha256(const u8* key, u32 key_length, const u8* message, u32 message_length, u8 output[32]) {
    u8 ipad[64], opad[64], inner[32];
    for (u32 i = 0; i < 64; ++i) {
        const u8 value = i < key_length ? key[i] : 0;
        ipad[i] = value ^ 0x36; opad[i] = value ^ 0x5C;
    }
    Sha256 ctx; sha_init(&ctx); sha_update(&ctx, ipad, 64); sha_update(&ctx, message, message_length); sha_final(&ctx, inner);
    sha_init(&ctx); sha_update(&ctx, opad, 64); sha_update(&ctx, inner, 32); sha_final(&ctx, output);
    volatile u8* wipe = inner;
    for (u32 i = 0; i < sizeof(inner); ++i) wipe[i] = 0;
    wipe = ipad; for (u32 i = 0; i < sizeof(ipad); ++i) wipe[i] = 0;
    wipe = opad; for (u32 i = 0; i < sizeof(opad); ++i) wipe[i] = 0;
    wipe = reinterpret_cast<volatile u8*>(&ctx); for (u32 i = 0; i < sizeof(ctx); ++i) wipe[i] = 0;
}

bool auth_random_bytes(u8* output, u32 length) {
    u32 eax = 1, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx; (void)edx;
    if (!(ecx & (1u << 30))) return false;
    while (length) {
        u32 value; u8 success = 0;
        for (u32 attempt = 0; attempt < 10 && !success; ++attempt)
            __asm__ volatile ("rdrand %0; setc %1" : "=&r"(value), "=qm"(success) : : "cc");
        if (!success) return false;
        for (u32 i = 0; i < 4 && length; ++i, --length) *output++ = static_cast<u8>(value >> (i*8));
    }
    return true;
}

void auth_pbkdf2_sha256(const u8* password, u32 password_length, const u8* salt, u32 salt_length, u32 iterations, u8 output[32]) {
    u8 block[68], u[32], result[32];
    for (u32 i = 0; i < salt_length; ++i) block[i] = salt[i];
    const u32 index = salt_length;
    block[index] = 0; block[index+1] = 0; block[index+2] = 0; block[index+3] = 1;
    hmac_sha256(password, password_length, block, salt_length + 4, u);
    for (u32 i = 0; i < 32; ++i) result[i] = u[i];
    if (iterations < 1) iterations = 1;
    for (u32 round = 1; round < iterations; ++round) {
        hmac_sha256(password, password_length, u, 32, u);
        for (u32 i = 0; i < 32; ++i) result[i] ^= u[i];
    }
    for (u32 i = 0; i < 32; ++i) output[i] = result[i];
    volatile u8* wipe = block;
    for (u32 i = 0; i < sizeof(block); ++i) wipe[i] = 0;
    wipe = u; for (u32 i = 0; i < sizeof(u); ++i) wipe[i] = 0;
    wipe = result; for (u32 i = 0; i < sizeof(result); ++i) wipe[i] = 0;
}

bool auth_constant_time_equal(const u8* a, const u8* b, u32 length) {
    u8 difference = 0;
    for (u32 i = 0; i < length; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}
