#include "paging.hpp"

static const u32 PAGE_SIZE = 4096;
static const u32 USER_BASE = 0x40000000;
static u32 page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static u32 page_tables[1024][1024] __attribute__((aligned(PAGE_SIZE)));

void paging_init() {
    for (u32 directory = 0; directory < 1024; ++directory) {
        page_directory[directory] = reinterpret_cast<u32>(&page_tables[directory][0]) | 0x03;
        for (u32 table = 0; table < 1024; ++table) {
            const u32 address = (directory << 22) | (table << 12);
            page_tables[directory][table] = address | 0x03;
        }
    }
    u32 cr3 = reinterpret_cast<u32>(page_directory);
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
    u32 cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000; // Enable paging and supervisor write protection.
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

bool paging_map_user(u32 virtual_address, u32 physical_address) {
    if ((virtual_address & (PAGE_SIZE - 1)) || (physical_address & (PAGE_SIZE - 1))) return false;
    if (virtual_address < USER_BASE || virtual_address >= USER_BASE + 0x00400000) return false;
    const u32 directory = virtual_address >> 22;
    const u32 table = (virtual_address >> 12) & 0x3FF;
    page_tables[directory][table] = (physical_address & 0xFFFFF000) | 0x07;
    page_directory[directory] |= 0x04;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return true;
}

void paging_unmap_user(u32 virtual_address) {
    if ((virtual_address & (PAGE_SIZE - 1)) || virtual_address < USER_BASE || virtual_address >= USER_BASE + 0x00400000) return;
    const u32 directory = virtual_address >> 22;
    const u32 table = (virtual_address >> 12) & 0x3FF;
    page_tables[directory][table] = virtual_address | 0x03;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
}
