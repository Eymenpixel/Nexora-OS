#include "memory.hpp"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

extern "C" u8 __kernel_end;

static const u32 PAGE_SIZE = 4096;
static const u32 MAX_PAGES = 1024 * 1024;
static u32 page_bitmap[MAX_PAGES / 32];
static u32 allocated_bitmap[MAX_PAGES / 32];
static u32 free_pages = 0;

struct __attribute__((packed)) Tag { u32 type; u32 size; };
struct __attribute__((packed)) MmapTag { u32 type; u32 size; u32 entry_size; u32 entry_version; };
struct __attribute__((packed)) MmapEntry { u64 address; u64 length; u32 type; u32 reserved; };

static void reserve_page(u32 page) {
    if (page >= MAX_PAGES) return;
    const u32 mask = 1u << (page % 32);
    if (!(page_bitmap[page / 32] & mask)) {
        page_bitmap[page / 32] |= mask;
        if (free_pages) --free_pages;
    }
}

static void release_page(u32 page) {
    if (page >= MAX_PAGES) return;
    const u32 mask = 1u << (page % 32);
    if (page_bitmap[page / 32] & mask) {
        page_bitmap[page / 32] &= ~mask;
        ++free_pages;
    }
}

void memory_init(u32 info_address) {
    for (u32 i = 0; i < MAX_PAGES / 32; ++i) { page_bitmap[i] = 0xFFFFFFFF; allocated_bitmap[i] = 0; }
    free_pages = 0;
    const u32 total_size = *reinterpret_cast<u32*>(info_address);
    u32 offset = 8;
    while (offset + sizeof(Tag) <= total_size) {
        Tag* tag = reinterpret_cast<Tag*>(info_address + offset);
        if (tag->size < sizeof(Tag) || offset + tag->size > total_size) break;
        if (tag->type == 6 && tag->size >= sizeof(MmapTag)) {
            MmapTag* map = reinterpret_cast<MmapTag*>(tag);
            if (map->entry_size >= sizeof(MmapEntry)) {
                u32 entry_offset = sizeof(MmapTag);
                while (entry_offset + map->entry_size <= map->size) {
                    MmapEntry* entry = reinterpret_cast<MmapEntry*>(reinterpret_cast<u32>(map) + entry_offset);
                    if (entry->type == 1) {
                        u64 begin = (entry->address + PAGE_SIZE - 1) / PAGE_SIZE;
                        u64 end = (entry->address + entry->length) / PAGE_SIZE;
                        if (begin < 256) begin = 256;
                        if (end > MAX_PAGES) end = MAX_PAGES;
                        for (u32 page = static_cast<u32>(begin); page < end; ++page) {
                            if (page_bitmap[page / 32] & (1u << (page % 32))) {
                                page_bitmap[page / 32] &= ~(1u << (page % 32));
                                ++free_pages;
                            }
                        }
                    }
                    entry_offset += map->entry_size;
                }
            }
        }
        if (tag->type == 0) break;
        offset = (offset + tag->size + 7) & ~7u;
    }

    reserve_page(0);
    const u32 kernel_end_page = (reinterpret_cast<u32>(&__kernel_end) + PAGE_SIZE - 1) / PAGE_SIZE;
    for (u32 page = 256; page < kernel_end_page; ++page) reserve_page(page);
    const u32 info_start = info_address / PAGE_SIZE;
    const u32 info_end = (info_address + total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (u32 page = info_start; page < info_end; ++page) reserve_page(page);
}

void* page_alloc() {
    for (u32 i = 256; i < MAX_PAGES; ++i) {
        if (!(page_bitmap[i / 32] & (1u << (i % 32)))) {
            page_bitmap[i / 32] |= 1u << (i % 32);
            allocated_bitmap[i / 32] |= 1u << (i % 32);
            if (free_pages) --free_pages;
            return reinterpret_cast<void*>(i * PAGE_SIZE);
        }
    }
    return 0;
}

bool page_free(void* address) {
    const u32 value = reinterpret_cast<u32>(address);
    if ((value & (PAGE_SIZE - 1)) || value < PAGE_SIZE * 256) return false;
    const u32 page = value / PAGE_SIZE;
    if (page >= MAX_PAGES || !(allocated_bitmap[page / 32] & (1u << (page % 32)))) return false;
    allocated_bitmap[page / 32] &= ~(1u << (page % 32));
    release_page(page);
    return true;
}

u32 memory_free_pages() { return free_pages; }
