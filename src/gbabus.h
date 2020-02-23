#ifndef __GBABUS_H__
#define __GBABUS_H__

#include "util.h"
#include "gbamem.h"

void init_gbabus(gbamem_t* new_mem);
byte gba_read_byte(uint32_t addr);
uint16_t gba_read16(uint32_t addr);
void gba_write_byte(uint32_t addr, byte value);
void gba_write16(uint32_t addr, uint16_t value);
uint32_t gba_read32(uint32_t addr);
void gba_write32(uint32_t address, uint32_t value);
#endif
