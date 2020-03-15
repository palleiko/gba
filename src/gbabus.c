#include "gbabus.h"
#include "ioreg_util.h"
#include "gbamem.h"
#include "common/log.h"
#include "gbabios.h"

gbamem_t* mem;

void init_gbabus(gbamem_t* new_mem) {
    mem = new_mem;
}

void write_byte_ioreg(word addr, byte value) {
    logwarn("Write to unknown (but valid) byte ioreg addr 0x%08X == 0x%02X", addr, value)
}

void write_half_ioreg(word addr, half value) {
    logwarn("Write to unknown (but valid) half ioreg addr 0x%08X == 0x%04X", addr, value)
}

void write_word_ioreg(word addr, word value) {
    // 0x04XX0800 is the only address that's mirrored.
    if ((addr & 0xFF00FFFFu) == 0x04000800u) {
        addr = 0xFF00FFFFu;
    }

    logwarn("Wrote 0x%08X to 0x%08X", value, addr)
    unimplemented(1, "io register write")
}

byte open_bus(word addr) {
    logfatal("Open bus unimplemented")
}

byte gba_read_byte(word addr) {
    addr &= ~(sizeof(byte) - 1);
    if (addr < GBA_BIOS_SIZE) { // BIOS
        return gbabios_read_byte(addr);
    } else if (addr < 0x01FFFFFF) {
        return open_bus(addr);
    } else if (addr < 0x03000000) { // EWRAM
        word index = (addr - 0x02000000) % 0x40000;
        return mem->ewram[index];
    } else if (addr < 0x04000000) { // IWRAM
        word index = (addr - 0x03000000) % 0x8000;
        return mem->iwram[index];
    } else if (addr < 0x04000400) {
        logfatal("Unimplemented: reading from ioreg")
    } else if (addr < 0x05000000) {
        logwarn("Tried to read from 0x%08X", addr)
        unimplemented(1, "Tried to read from unused portion of general internal memory")
    } else if (addr < 0x08000000) {
        logwarn("Tried to read from 0x%08X", addr)
        unimplemented(1, "Read from internal display memory address")
    } else if (addr < 0x0E00FFFF) {
        // Cartridge
        word adjusted = addr - 0x08000000;
        if (adjusted > mem->rom_size) {
            return open_bus(addr);
        } else {
            return mem->rom[adjusted];
        }
    }

    return open_bus(addr);
}

half gba_read_half(word addr) {
    addr &= ~(sizeof(half) - 1);
    byte lower = gba_read_byte(addr);
    byte upper = gba_read_byte(addr + 1);

    return (upper << 8u) | lower;
}

void gba_write_byte(word addr, byte value) {
    addr &= ~(sizeof(byte) - 1);
    if (addr < GBA_BIOS_SIZE) {
        logfatal("Tried to write to the BIOS!")
    } else if (addr < 0x01FFFFFF) {
        logfatal("Tried to write to unused section of RAM in between bios and WRAM")
    } else if (addr < 0x03000000) { // EWRAM
        word index = (addr - 0x02000000) % 0x40000;
        mem->ewram[index] = addr;
    } else if (addr < 0x04000000) { // IWRAM
        word index = (addr - 0x03000000) % 0x8000;
        mem->iwram[index] = value;
    } else if (addr < 0x04000400) {
        write_byte_ioreg(addr, value);
    } else if (addr < 0x05000000) {
        logwarn("Tried to write to 0x%08X", addr)
        unimplemented(1, "Tried to write to unused portion of general internal memory")
    } else if (addr < 0x06000000) { // Palette RAM
        word index = (addr - 0x5000000) % 0x400;
        mem->pram[index] = value;
    } else if (addr < 0x07000000) {
        logfatal("VRAM write")
    } else if (addr < 0x08000000) {
        logwarn("Tried to write to 0x%08X", addr)
        unimplemented(1, "Write to internal display memory address")
    } else if (addr < 0x0E00FFFF) {
        logwarn("Tried to write to 0x%08X", addr)
        unimplemented(1, "Write to cartridge address")
    } else {
        logfatal("Something's up, we reached the end of gba_write_byte without writing a value! addr: 0x%08X", addr)
    }
}

void gba_write_half(word address, half value) {
    address &= ~(sizeof(half) - 1);
    if (is_ioreg(address)) {
        byte ioreg_size = get_ioreg_size_for_addr(address);
        unimplemented(ioreg_size > sizeof(half), "writing to a too-large ioreg from gba_write_half")
        if (ioreg_size == sizeof(half)) {
            write_half_ioreg(address, value);
            return;
        } else if (ioreg_size == 0) {
            // Unused io register
            logwarn("Unused half size ioregister!")
            return;
        }
    }

    byte lower = value & 0xFFu;
    byte upper = (value & 0xFF00u) >> 8u;
    gba_write_byte(address, lower);
    gba_write_byte(address + 1, upper);
}

word gba_read_word(word addr) {
    addr &= ~(sizeof(word) - 1);
    word lower = gba_read_half(addr);
    word upper = gba_read_half(addr + 2);

    return (upper << 16u) | lower;
}

void gba_write_word(word address, word value) {
    address &= ~(sizeof(word) - 1);
    if (is_ioreg(address)) {
        byte ioreg_size = get_ioreg_size_for_addr(address);
        if(ioreg_size == sizeof(word)) {
            write_word_ioreg(address, value);
            logfatal("Writing a word to word-size ioreg")
        } else if (ioreg_size == 0) {
            logwarn("Unused word size ioregister!")
            // Unused io register
            return;
        }
        // Otherwise, it'll be smaller than a word, and we'll write each part to the respective registers.
    }

    half lower = (value & 0xFFFFu);
    half upper = (value & 0xFFFF0000u) >> 16u;

    gba_write_half(address, lower);
    gba_write_half(address + 2, upper);
}

