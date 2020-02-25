#include <stdlib.h>
#include <stdbool.h>
#include "arm7tdmi.h"
#include "log.h"
#include "arm_instr.h"

void fill_pipe(arm7tdmi_t* state) {
    state->pipeline[0] = state->read_word(state->pc);
    state->pc += 4;
    state->pipeline[1] = state->read_word(state->pc);
    state->pc += 4;

    logdebug("Filling the instruction pipeline: 0x%08X = 0x%08X / 0x%08X = 0x%08X",
             state->pc - 8,
             state->pipeline[0],
             state->pc - 4,
             state->pipeline[1])
}

arm7tdmi_t* init_arm7tdmi(byte (*read_byte)(word),
                          half (*read_half)(word),
                          word (*read_word)(word),
                          void (*write_byte)(word, byte),
                          void (*write_half)(word, half),
                          void (*write_word)(word, word)) {
    arm7tdmi_t* state = malloc(sizeof(arm7tdmi_t));

    state->read_byte = read_byte;
    state->read_half = read_half;
    state->read_word = read_word;
    state->write_byte = write_byte;
    state->write_half = write_half;
    state->write_word = write_word;

    state->pc = 0x08000000;

    state->cpsr.raw = 0;

    fill_pipe(state);
    return state;
}

bool check_cond(arminstr_t* instr) {
    switch (instr->parsed.cond) {
        case AL:
            return true;
        default:
            logfatal("Unimplemented COND: %d", instr->parsed.cond);
    }
}

int this_step_ticks = 0;

void tick(int ticks) {
    this_step_ticks += ticks;
}

arminstr_t next_instr(arm7tdmi_t* state) {
    // TODO handle thumb mode

    arminstr_t instr;
    instr.raw = state->pipeline[0];
    state->pipeline[0] = state->pipeline[1];
    state->pipeline[1] = state->read_word(state->pc);

    return instr;
}

void set_register(arm7tdmi_t* state, word index, word newvalue) {
    if (index > 12) {
        logfatal("Tried to set a register > r12 - this has the possibility of being different depending on the mode, but that isn't implemented yet.")
    }
    logdebug("Set r%d to 0x%08X", index, newvalue)
    state->r[index] = newvalue;
}

word get_register(arm7tdmi_t* state, word index) {
    if (index > 12) {
        logfatal("Tried to get a register > r12 - this has the possibility of being different depending on the mode, but that isn't implemented yet.")
    }

    logdebug("Read the value of r%d: 0x%08X", index, state->r[index])
    return state->r[index];
}


void psr_transfer(arm7tdmi_t* state,
                  bool immediate,
                  unsigned int dt_opcode,
                  unsigned int dt_rn,
                  unsigned int dt_rd,
                  unsigned int dt_operand2) {
    union {
        struct {
            bool msr:1; // if 0, mrs, if 1, msr
            bool spsr:1; // if 0, cpsr, if 1, spsr_current mode
            unsigned:2;
        };
        unsigned raw:4;
    } opcode;
    opcode.raw = dt_opcode;

    unimplemented(!opcode.msr, "MRS mode unimplemented.")
    unimplemented(opcode.spsr, "SPSR not implemented")

    if (opcode.msr) {
        union {
            struct {
                bool f:1; // Write to flags field
                bool s:1; // Write to status field
                bool x:1; // Write to extension field
                bool c:1; // Write to control field
            };
            unsigned raw:4;
        } field_masks;
        field_masks.raw = dt_rn; // field masks come from the "rn" field in data processing

        if (immediate) {
            logfatal("Immediate not implemented")
        }
        else {
            unsigned int source_register = dt_operand2 & 0b1111u;
            // Debug
            printf("MSR CPSR_");
            if (field_masks.c) { printf("c"); }
            if (field_masks.x) { printf("x"); }
            if (field_masks.s) { printf("s"); }
            if (field_masks.f) { printf("f"); }
            printf(", r%d\n", source_register);
            // http://problemkaputt.de/gbatek.htm#armopcodespsrtransfermrsmsr

            word source_data = get_register(state, source_register);
            logdebug("Source data: 0x%08X", source_data);
            logfatal("I know how to get the instruction, but not how to execute it.")
        }
    }
    else {
        // MRS
        logfatal("Hello! This is an MRS instruction!")
    }

}

// http://problemkaputt.de/gbatek.htm#armopcodesdataprocessingalu
void data_processing(arm7tdmi_t* state,
                     word immediate_operand2,
                     word rd,
                     word rn,
                     bool s,
                     bool immediate,
                     word opcode) {
    // If it's one of these opcodes, and the s flag isn't set, this is actually a psr transfer op.
    if (!s && opcode >= 0x8 && opcode <= 0xb) { // TODO optimize with masks (opcode>>8) & 0b1100 == 0b1100 ?
        return psr_transfer(state, immediate, opcode, rn, rd, immediate_operand2);
    }

    unimplemented(s, "updating condition codes flag in data processing")

    word operand2;

    // Operand2 comes from an immediate value
    if (immediate) {
        // Because this is immediate mode, we gotta do stuff with the operand
        operand2 = immediate_operand2 & 0xFFu; // Last 8 bits of operand2 are the pre-shift value

        // first 4 bytes * 7 are the shift value
        // Only shift by 7 because we were going to multiply it by 2 anyway
        word shift = (immediate_operand2 & 0xF00u) >> 7u;

        operand2 &= 31u;
        operand2 = (operand2 >> shift) | (operand2 << (-shift & 31u));
    }
    // Operand2 comes from another register
    else {
        //logfatal("In a data processing instruction, NOT using immediate, time to implement it.")
        union {
            struct {
                struct {
                    unsigned:7; // common flags
                    unsigned shift_amount:5;
                } shift_immediate;
                struct {
                    unsigned:8; // common flags
                    unsigned rs:4;
                } shift_register;
                // Common to both
                struct {
                    unsigned rm:4;
                    bool r:1; // 1: shift by register, 0, shift by immediate.
                    unsigned shift_type:2;
                };
            };
            unsigned raw:12;
        } nonimmediate_flags;

        byte shift_amount;

        // Shift by register
        if (nonimmediate_flags.r) {
            shift_amount = get_register(state, nonimmediate_flags.shift_register.rs);
        }
        // Shift by immediate
        else {
            shift_amount = nonimmediate_flags.shift_immediate.shift_amount;
        }

        unimplemented(1, "I've done part of this, still work to do.")
    }


    switch(opcode) {
        case 0xC: // OR logical: Rd = Rn OR Op2
            set_register(state, rd, get_register(state, rn) | operand2);
            break;
        case 0xD: // MOV: Rd = Op2
            set_register(state, rd, operand2);
            break;
        default:
            logfatal("DATA_PROCESSING: unknown opcode: 0x%X", opcode)
    }
}

// http://problemkaputt.de/gbatek.htm#armopcodesmemorysingledatatransferldrstrpld
void single_data_transfer(arm7tdmi_t* state,
                          unsigned int offset,
                          unsigned int rd, // dest if this is LDR, source if this is STR
                          unsigned int rn,
                          bool l,   // 0 == str, 1 == ldr
                          bool w,   // different meanings depending on state of P (writeback)
                          bool b,   // (byte) when 0, transfer word, when 1, transfer byte
                          bool up,  // When 0, subtract offset from base, when 1, add to base
                          bool pre, // when 0, offset after transfer, when 1, before transfer.
                          bool immediate_offset_type) { //  When 0, Immediate as Offset
                                                        //  When 1, Register shifted by Immediate as Offset
    logdebug("l: %d w: %d b: %d u: %d p: %d i: %d", l, w, b, up, pre, immediate_offset_type)
    logdebug("rn: %d rd: %d, offset: 0x%03X", rn, rd, offset)
    unimplemented(immediate_offset_type, "immediate_offset_type == 1 in single_data_transfer")
    unimplemented(l, "LDR")
    unimplemented(w, "w flag")
    unimplemented(!up, "down (subtract from base)")
    unimplemented(!pre, "post-transfer offset")
    unimplemented(rn == 15, "special case where rn == r15")

    word address = get_register(state, rn) + offset;

    logdebug("I'm gonna save r%d to 0x%02X", rd, address)
    state->write_word(address, get_register(state, rd));
}


void branch(arm7tdmi_t* state, word offset, bool link) {
    bool thumb = offset & 1u;
    unimplemented(thumb, "THUMB mode")
    bool negative = (offset & 0b100000000000000000000000u) > 0;
    if (negative) {
        offset = ~offset + 1;
        logfatal("Encountered a branch with a negative offset. Make sure this is doing the right thing!")
    }
    loginfo("My offset is %d", offset << 2u)

    if (link) {
        logfatal("Branch-with-link isn't implemented yet.")
    }

    word newpc = (state->pc) + (offset << 2u);
    logdebug("Hold on to your hats, we're jumping to 0x%02X", newpc)
    state->pc = newpc;
    fill_pipe(state);
}

int arm7tdmi_step(arm7tdmi_t* state) {
    this_step_ticks = 0;
    arminstr_t instr = next_instr(state);
    logwarn("adjusted pc: 0x%04X read: 0x%04X", state->pc - 8, instr.raw)
    logdebug("cond: %d", instr.parsed.cond)
    if (check_cond(&instr)) {
        arm_instr_type_t type = get_instr_type(&instr);
        switch (type) {
            case DATA_PROCESSING:
                data_processing(state,
                                instr.parsed.DATA_PROCESSING.operand2,
                                instr.parsed.DATA_PROCESSING.rd,
                                instr.parsed.DATA_PROCESSING.rn,
                                instr.parsed.DATA_PROCESSING.s,
                                instr.parsed.DATA_PROCESSING.immediate,
                                instr.parsed.DATA_PROCESSING.opcode);
                break;
            case MULTIPLY:
                unimplemented(1, "MULTIPLY instruction type")
            case MULTIPLY_LONG:
                logfatal("Unimplemented instruction type: MULTIPLY_LONG")
            case SINGLE_DATA_SWAP:
                logfatal("Unimplemented instruction type: SINGLE_DATA_SWAP")
            case BRANCH_EXCHANGE:
                logfatal("Unimplemented instruction type: BRANCH_EXCHANGE")
            case HALFWORD_DT_RO:
                logfatal("Unimplemented instruction type: HALFWORD_DT_RO")
            case HALFWORD_DT_IO:
                logfatal("Unimplemented instruction type: HALFWORD_DT_IO")
            case SINGLE_DATA_TRANSFER:
                single_data_transfer(state,
                                     instr.parsed.SINGLE_DATA_TRANSFER.offset,
                                     instr.parsed.SINGLE_DATA_TRANSFER.rd,
                                     instr.parsed.SINGLE_DATA_TRANSFER.rn,
                                     instr.parsed.SINGLE_DATA_TRANSFER.l,
                                     instr.parsed.SINGLE_DATA_TRANSFER.w,
                                     instr.parsed.SINGLE_DATA_TRANSFER.b,
                                     instr.parsed.SINGLE_DATA_TRANSFER.u,
                                     instr.parsed.SINGLE_DATA_TRANSFER.p,
                                     instr.parsed.SINGLE_DATA_TRANSFER.i);
                break;
            case UNDEFINED:
                logfatal("Unimplemented instruction type: UNDEFINED")
            case BLOCK_DATA_TRANSFER:
                logfatal("Unimplemented instruction type: BLOCK_DATA_TRANSFER")
            case BRANCH:
                branch(state, instr.parsed.BRANCH.offset, instr.parsed.BRANCH.l);
                state->pc -= 4; // This is to correct for the state->pc+=4 that happens after this switch
                break;
            case COPROCESSOR_DATA_TRANSFER:
                logfatal("Unimplemented instruction type: COPROCESSOR_DATA_TRANSFER")
            case COPROCESSOR_DATA_OPERATION:
                logfatal("Unimplemented instruction type: COPROCESSOR_DATA_OPERATION")
            case COPROCESSOR_REGISTER_TRANSFER:
                logfatal("Unimplemented instruction type: COPROCESSOR_REGISTER_TRANSFER")
            case SOFTWARE_INTERRUPT:
                logfatal("Unimplemented instruction type: SOFTWARE_INTERRUPT")
        }
        state->pc += 4;
    }
    else { // Cond told us not to execute this instruction
        tick(1);
    }
    return this_step_ticks;
}
