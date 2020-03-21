#include "gbabus.h"
#include "ioreg_util.h"
#include "gbamem.h"
#include "common/log.h"
#include "gbabios.h"

gbamem_t* mem;
arm7tdmi_t* cpu;
gba_ppu_t* ppu;

gbabus_t state;

void write_ime(bool value) {
    state.interrupt_master_enable = value;
    if (state.interrupt_master_enable) {
        logwarn("Enabled interrupts!")
    } else {
        logwarn("Disabled all interrupts")
    }
}

void init_gbabus(gbamem_t* new_mem, arm7tdmi_t* new_cpu, gba_ppu_t* new_ppu) {
    mem = new_mem;
    cpu = new_cpu;
    ppu = new_ppu;

    state.interrupt_master_enable = false;
    state.interrupt_enable.raw = 0;
}

void write_byte_ioreg(word addr, byte value) {
    word regnum = addr & 0xFFF;
    switch (regnum) {
        case IO_IME:
            write_ime(value & 1);
            break;
        default:
            logfatal("Write to unknown (but valid) byte ioreg addr 0x%08X == 0x%02X", addr, value)
    }
}

void write_half_ioreg(word addr, half value) {
    word regnum = addr & 0xFFF;
    switch (regnum) {
        case IO_DISPCNT:
            write_dispcnt(ppu, value);
            break;
        case IO_UNDOCUMENTED_GREEN_SWAP:
            logwarn("Ignoring write to Green Swap register")
            break;
        case IO_DISPSTAT: {
            half dispstat = value & 0b1111111111111000;
            ppu->DISPSTAT.raw &= 0b0000000000000111;
            ppu->DISPSTAT.raw |= dispstat;
            break;
        }
        case IO_VCOUNT:
            logwarn("Writing to read-only VCOUNT register, ignoring!")
            break;
        case IO_BG0CNT:
            ppu->BG0CNT.raw = value;
            break;
        case IO_BG1CNT:
            ppu->BG1CNT.raw = value;
            break;
        case IO_BG2CNT:
            ppu->BG2CNT.raw = value;
            break;
        case IO_BG3CNT:
            ppu->BG3CNT.raw = value;
            break;
        case IO_BG0HOFS:
            ppu->BG0HOFS.raw = value;
            break;
        case IO_BG1HOFS:
            ppu->BG1HOFS.raw = value;
            break;
        case IO_BG2HOFS:
            ppu->BG2HOFS.raw = value;
            break;
        case IO_BG3HOFS:
            ppu->BG3HOFS.raw = value;
            break;
        case IO_BG0VOFS:
            ppu->BG0VOFS.raw = value;
            break;
        case IO_BG1VOFS:
            ppu->BG1VOFS.raw = value;
            break;
        case IO_BG2VOFS:
            ppu->BG2VOFS.raw = value;
            break;
        case IO_BG3VOFS:
            ppu->BG3VOFS.raw = value;
            break;
        case IO_BG2PA:
            ppu->BG2PA.raw = value;
            break;
        case IO_BG2PB:
            ppu->BG2PB.raw = value;
            break;
        case IO_BG2PC:
            ppu->BG2PC.raw = value;
            break;
        case IO_BG2PD:
            ppu->BG2PD.raw = value;
            break;
        case IO_IE:
            state.interrupt_enable.raw = value;
            break;
        case IO_IF:
            logwarn("Ignoring write to IF register: 0x%04X", value)
            break;
        case IO_WAITCNT:
            logwarn("Ignoring write to WAITCNT register: 0x%04X", value)
            break;
        case IO_IME:
            write_ime(value & 1);
            break;
        default:
            logfatal("Write to unknown (but valid) half ioreg addr 0x%08X == 0x%04X", addr, value)
    }
}

word* get_word_ioreg_ptr(word addr) {
    unimplemented(get_ioreg_size_for_addr(addr) != sizeof(word), "Trying to get the address of a wrong-sized word ioreg")
    logfatal("Tried to get the address of an unknown (but valid) word ioreg addr: 0x%08X", addr)
}

void write_word_ioreg(word addr, word value) {
    // 0x04XX0800 is the only address that's mirrored.
    if ((addr & 0xFF00FFFFu) == 0x04000800u) {
        addr = 0xFF00FFFFu;
    }

    logwarn("Wrote 0x%08X to 0x%08X", value, addr)
    unimplemented(1, "io register write")
}

void write_word_ioreg_masked(word addr, word value, word mask) {
    word* ioreg = get_word_ioreg_ptr(addr);
    *ioreg &= (~mask);
    *ioreg |= (value & mask);
    logfatal("masked io register write! addr: 0x%08X value: 0x%08X mask: 0x%08X", addr, value, mask)
}

word read_word_ioreg(word addr) {
    logfatal("read from unknown (but valid) word ioreg addr 0x%08x", addr)
}

half read_half_ioreg(word addr) {
    word regnum = addr & 0xFFF;
    switch (regnum) {
        case IO_VCOUNT:
            return ppu->y;
        default:
            logfatal("read from unknown (but valid) half ioreg addr 0x%08x", addr)
    }
}

bool is_open_bus(word address) {
    if (address < GBA_BIOS_SIZE) {
        return false;
    } else if (address < 0x01FFFFFF) {
        return true;
    } else if (address < 0x08000000) {
        return false;
    } else if (address < 0x08000000 + mem->rom_size) {
        return false;
    } else {
        return true;
    }
}

word open_bus(word addr) {
    unimplemented(cpu->cpsr.thumb, "Open bus unimplemented in thumb mode")
    word result = cpu->pipeline[1];
    result >>= ((addr & 0b11u) << 3u);

    logwarn("RETURNING FROM OPEN BUS AT ADDRESS 0x%08X: 0x%08X", addr, result);
    return result;
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
    } else if (addr < 0x06000000) { // Palette RAM
        word index = (addr - 0x5000000) % 0x400;
        return mem->pram[index];
    } else if (addr < 0x07000000) {
        word index = addr - 0x06000000;
        unimplemented(index > 0x17FFF, "VRAM mirroring")
        return mem->vram[index];
    } else if (addr < 0x08000000) {
        word index = addr - 0x08000000;
        index %= OAM_SIZE;
        return mem->oam[index];
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

half gba_read_half(word address) {
    address &= ~(sizeof(half) - 1);
    if (is_ioreg(address)) {
        byte ioreg_size = get_ioreg_size_for_addr(address);
        unimplemented(ioreg_size > sizeof(half), "Reading from a too-large ioreg from gba_read_half")
        if (ioreg_size == sizeof(half)) {
            return read_half_ioreg(address);
        } else if (ioreg_size == 0) {
            // Unused io register
            logfatal("Read from unused half size ioregister!")
        }
    }

    if (is_open_bus(address)) {
        return open_bus(address);
    }
    byte lower = gba_read_byte(address);
    byte upper = gba_read_byte(address + 1);

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
        word index = addr - 0x06000000;
        unimplemented(index > 0x17FFF, "VRAM mirroring")
        mem->vram[index] = value;
    } else if (addr < 0x08000000) {
        word index = addr - 0x08000000;
        index %= OAM_SIZE;
        mem->oam[index] = value;
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
        if (ioreg_size == sizeof(word)) {
            word offset = (address % sizeof(word)) * 8;
            word shifted_value = value;
            shifted_value <<= offset;
            write_word_ioreg_masked(address, shifted_value, 0xFFFF << offset);
        } else if (ioreg_size == sizeof(half)) {
            write_half_ioreg(address, value);
            return;
        } else if (ioreg_size == 0) {
            // Unused io register
            logwarn("Unused half size ioregister 0x%08X", address)
            return;
        }
    }

    byte lower = value & 0xFFu;
    byte upper = (value & 0xFF00u) >> 8u;
    gba_write_byte(address, lower);
    gba_write_byte(address + 1, upper);
}

word gba_read_word(word address) {
    address &= ~(sizeof(word) - 1);

    if (is_ioreg(address)) {
        byte ioreg_size = get_ioreg_size_for_addr(address);
        if(ioreg_size == sizeof(word)) {
            return read_word_ioreg(address);
        } else if (ioreg_size == 0) {
            logfatal("Read from unused word size ioregister!")
        }
        // Otherwise, it'll be smaller than a word, and we'll read each part from the respective registers.
    }
    if (is_open_bus(address)) {
        return open_bus(address);
    }
    word lower = gba_read_half(address);
    word upper = gba_read_half(address + 2);

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

