#pragma once

typedef unsigned int u32;
typedef unsigned long long u64;

void memory_init(u32 multiboot_info);
void* page_alloc();
bool page_free(void* address);
u32 memory_free_pages();
