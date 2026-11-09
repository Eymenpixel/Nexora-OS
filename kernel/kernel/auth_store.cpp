#include "auth_store.hpp"

typedef unsigned short u16;

static const u16 ATA_DATA = 0x1F0;
static const u16 ATA_SECCOUNT = 0x1F2;
static const u16 ATA_LBA0 = 0x1F3;
static const u16 ATA_LBA1 = 0x1F4;
static const u16 ATA_LBA2 = 0x1F5;
static const u16 ATA_DRIVE = 0x1F6;
static const u16 ATA_STATUS = 0x1F7;
static const u32 AUTH_LBA_A = 8;
static const u32 AUTH_LBA_B = 9;

static void outb(u16 port, u8 value) { __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static u8 inb(u16 port) { u8 v; __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static void outw(u16 port, u16 value) { __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port)); }
static u16 inw(u16 port) { u16 v; __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port)); return v; }

static bool wait_ready(bool require_data) {
    for (u32 timeout = 0; timeout < 1000000; ++timeout) {
        const u8 status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF || (status & 0x21)) return false;
        if (!(status & 0x80) && (!require_data || (status & 0x08))) return true;
    }
    return false;
}

static bool ata_sector(u32 lba, u8* buffer, bool write) {
    outb(ATA_DRIVE, static_cast<u8>(0xE0 | ((lba >> 24) & 0x0F)));
    for (u32 i = 0; i < 4; ++i) (void)inb(ATA_STATUS);
    if (!wait_ready(false)) return false;
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA0, static_cast<u8>(lba));
    outb(ATA_LBA1, static_cast<u8>(lba >> 8));
    outb(ATA_LBA2, static_cast<u8>(lba >> 16));
    outb(ATA_STATUS, write ? 0x30 : 0x20);
    if (!wait_ready(true)) return false;
    if (write) {
        for (u32 i = 0; i < 256; ++i) outw(ATA_DATA, static_cast<u16>(buffer[i*2] | (u16(buffer[i*2+1]) << 8)));
        outb(ATA_STATUS, 0xE7);
        return wait_ready(false);
    }
    for (u32 i = 0; i < 256; ++i) {
        const u16 value = inw(ATA_DATA);
        buffer[i*2] = static_cast<u8>(value);
        buffer[i*2+1] = static_cast<u8>(value >> 8);
    }
    return true;
}

static u32 crc32(const u8* bytes, u32 length) {
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (u32 bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
    }
    return ~crc;
}

static bool valid_record(const u8* sector, AuthRecord* record) {
    const AuthRecord* candidate = reinterpret_cast<const AuthRecord*>(sector);
    static const char magic[8] = {'B','B','A','U','T','H','1',0};
    for (u32 i = 0; i < 8; ++i) if (candidate->magic[i] != magic[i]) return false;
    if (candidate->version != 1 || candidate->iterations != 600000) return false;
    if (candidate->checksum != crc32(sector, sizeof(AuthRecord) - 4)) return false;
    *record = *candidate;
    return true;
}

static bool blank_sector(const u8* sector) {
    bool zero = true, ones = true;
    for (u32 i = 0; i < 512; ++i) { if (sector[i] != 0) zero = false; if (sector[i] != 0xFF) ones = false; }
    return zero || ones;
}

AuthStoreState auth_store_load(AuthRecord* record) {
    u8 a[512], b[512];
    if (!ata_sector(AUTH_LBA_A, a, false) || !ata_sector(AUTH_LBA_B, b, false)) return AUTH_STORE_OFFLINE;
    AuthRecord record_a, record_b;
    const bool valid_a = valid_record(a, &record_a);
    const bool valid_b = valid_record(b, &record_b);
    if (valid_a && valid_b) { *record = record_a.generation >= record_b.generation ? record_a : record_b; return AUTH_STORE_READY; }
    if (valid_a) { *record = record_a; return AUTH_STORE_READY; }
    if (valid_b) { *record = record_b; return AUTH_STORE_READY; }
    if (blank_sector(a) && blank_sector(b)) return AUTH_STORE_EMPTY;
    return AUTH_STORE_CORRUPT;
}

bool auth_store_create(const AuthRecord* source) {
    u8 sector[512];
    AuthRecord current;
    const AuthStoreState state = auth_store_load(&current);
    if (state != AUTH_STORE_EMPTY) return false;
    for (u32 i = 0; i < sizeof(sector); ++i) sector[i] = 0;
    AuthRecord record = *source;
    static const char magic[8] = {'B','B','A','U','T','H','1',0};
    for (u32 i = 0; i < 8; ++i) record.magic[i] = magic[i];
    record.version = 1; record.generation = 1;
    record.checksum = crc32(reinterpret_cast<const u8*>(&record), sizeof(AuthRecord) - 4);
    for (u32 i = 0; i < sizeof(AuthRecord); ++i) sector[i] = reinterpret_cast<const u8*>(&record)[i];
    if (!ata_sector(AUTH_LBA_A, sector, true)) return false;
    record.generation = 2;
    record.checksum = crc32(reinterpret_cast<const u8*>(&record), sizeof(AuthRecord) - 4);
    for (u32 i = 0; i < sizeof(sector); ++i) sector[i] = 0;
    for (u32 i = 0; i < sizeof(AuthRecord); ++i) sector[i] = reinterpret_cast<const u8*>(&record)[i];
    // Slot A is already a valid committed record; a failed redundant write must not strand setup.
    (void)ata_sector(AUTH_LBA_B, sector, true);
    return true;
}
