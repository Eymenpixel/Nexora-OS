#pragma once

typedef unsigned int u32;

void paging_init();
bool paging_map_user(u32 virtual_address, u32 physical_address);
void paging_unmap_user(u32 virtual_address);
