#include <stdlib.h>
#include <stdbool.h>
#include "arm7tdmi.h"
#include "../common/log.h"

#include "arm_instr/arm_instr.h"
#include "arm_instr/data_processing.h"
#include "arm_instr/single_data_transfer.h"
#include "arm_instr/branch.h"
#include "arm_instr/block_data_transfer.h"
#include "arm_instr/status_transfer.h"
#include "arm_instr/halfword_data_transfer.h"

#include "thumb_instr/thumb_instr.h"
#include "thumb_instr/immediate_operations.h"
#include "thumb_instr/high_register_operations.h"
#include "thumb_instr/load_address.h"
#include "arm_instr/multiply.h"
#include "arm_instr/single_data_swap.h"
#include "thumb_instr/move_shifted_register.h"
#include "thumb_instr/alu_operations.h"
#include "thumb_instr/load_store_halfword.h"
#include "thumb_instr/load_store.h"
#include "thumb_instr/pc_relative_load.h"
#include "thumb_instr/sp_relative_load_store.h"
#include "thumb_instr/add_offset_to_stack_pointer.h"
#include "thumb_instr/push_pop_registers.h"
#include "thumb_instr/multiple_load_store.h"
#include "thumb_instr/conditional_branch.h"
#include "thumb_instr/thumb_software_interrupt.h"
#include "thumb_instr/unconditional_branch.h"
#include "thumb_instr/long_branch_link.h"
#include "thumb_instr/add_subtract.h"
#include "../graphics/debug.h"
#include "../disassemble.h"
#include "arm_instr/arm_software_interrupt.h"

const char MODE_NAMES[32][11] = {
"UNKNOWN",    // 0b00000
"UNKNOWN",    // 0b00001
"UNKNOWN",    // 0b00010
"UNKNOWN",    // 0b00011
"UNKNOWN",    // 0b00100
"UNKNOWN",    // 0b00101
"UNKNOWN",    // 0b00110
"UNKNOWN",    // 0b00111
"UNKNOWN",    // 0b01000
"UNKNOWN",    // 0b01001
"UNKNOWN",    // 0b01010
"UNKNOWN",    // 0b01011
"UNKNOWN",    // 0b01100
"UNKNOWN",    // 0b01101
"UNKNOWN",    // 0b01110
"UNKNOWN",    // 0b01111
"USER",       // 0b10000
"FIQ",        // 0b10001
"IRQ",        // 0b10010
"SUPERVISOR", // 0b10011
"UNKNOWN",    // 0b10100
"UNKNOWN",    // 0b10101
"UNKNOWN",    // 0b10110
"ABORT",      // 0b10111
"UNKNOWN",    // 0b11000
"UNKNOWN",    // 0b11001
"UNKNOWN",    // 0b11010
"UNDEFINED",  // 0b11011
"UNKNOWN",    // 0b11100
"UNKNOWN",    // 0b11101
"UNKNOWN",    // 0b11110
"SYS",        // 0b11111
};

void fill_pipe(arm7tdmi_t* state) {
    if (state->cpsr.thumb) {
        state->pipeline[0] = state->read_half(state->pc);
        state->pc += 2;
        state->pipeline[1] = state->read_half(state->pc);

        logdebug("[THM] Filling the instruction pipeline: 0x%08X = 0x%04X / 0x%08X = 0x%04X",
                 state->pc - 2,
                 state->pipeline[0],
                 state->pc,
                 state->pipeline[1])
    } else {
        state->pipeline[0] = state->read_word(state->pc);
        state->pc += 4;
        state->pipeline[1] = state->read_word(state->pc);

        logdebug("[ARM] Filling the instruction pipeline: 0x%08X = 0x%08X / 0x%08X = 0x%08X",
                 state->pc - 4,
                 state->pipeline[0],
                 state->pc,
                 state->pipeline[1])
    }
}

void set_pc(arm7tdmi_t* state, word new_pc) {
    if (new_pc & 1u) {
        state->cpsr.thumb = true;
        new_pc &= ~1u; // Unset thumb bit as it's a flag, not really part of the address
    } else if (state->cpsr.thumb && (new_pc & 1u) == 0u) {
        state->cpsr.thumb = false;
    }

    if (state->cpsr.thumb && new_pc % 2 != 0) {
        logfatal("Attempted to jump in THUMB mode to a non-half aligned address 0x%08X!", new_pc)
    } else if (!state->cpsr.thumb && new_pc % 4 != 0) {
        logwarn("Attempted to jump in ARM mode to a non-word aligned address 0x%08X!", new_pc)
        new_pc &= 0xFFFFFFFC; // Correct alignment
        logwarn("Corrected it to 0x%08X!", new_pc)
    }

    state->pc = new_pc;
    fill_pipe(state);
}

arm7tdmi_t* init_arm7tdmi(byte (*read_byte)(word),
                          half (*read_half)(word),
                          word (*read_word)(word),
                          void (*write_byte)(word, byte),
                          void (*write_half)(word, half),
                          void (*write_word)(word, word)) {
    arm7tdmi_t* state = malloc(sizeof(arm7tdmi_t));

    state->read_byte  = read_byte;
    state->read_half  = read_half;
    state->read_word  = read_word;
    state->write_byte = write_byte;
    state->write_half = write_half;
    state->write_word = write_word;

    state->pc       = 0x00000000;
    state->sp       = 0x03007F00;
    state->lr       = 0x08000000;
    state->cpsr.raw = 0x0000005F;

    for (int r = 0; r < 13; r++) {
        state->r[r] = 0;
    }

    state->sp_fiq = 0x00000000;
    state->sp_svc = 0x00000000;
    state->sp_abt = 0x00000000;
    state->sp_irq = 0x00000000;
    state->sp_und = 0x00000000;

    state->lr_fiq = 0x00000000;
    state->lr_svc = 0x00000000;
    state->lr_abt = 0x00000000;
    state->lr_irq = 0x00000000;
    state->lr_und = 0x00000000;

    state->highreg_fiq[0] = 0;
    state->highreg_fiq[1] = 0;
    state->highreg_fiq[2] = 0;
    state->highreg_fiq[3] = 0;
    state->highreg_fiq[4] = 0;

    state->irq = false;
    state->halt = false;

    fill_pipe(state);
    return state;
}

// EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV
bool check_cond(arm7tdmi_t* state, arminstr_t* instr) {
    bool passed = false;
    switch (instr->parsed.cond) {
        case EQ:
            passed = state->cpsr.Z == 1;
            break;
        case NE:
            passed = state->cpsr.Z == 0;
            break;
        case CS:
            passed = state->cpsr.C == 1;
            break;
        case CC:
            passed = state->cpsr.C == 0;
            break;
        case MI:
            passed = state->cpsr.N == 1;
            break;
        case PL:
            passed = state->cpsr.N == 0;
            break;
        case VS:
            passed = state->cpsr.V == 1;
            break;
        case VC:
            passed = state->cpsr.V == 0;
            break;
        case HI:
            passed = state->cpsr.C == 1 && state->cpsr.Z == 0;
            break;
        case LS:
            passed = state->cpsr.C == 0 || state->cpsr.Z == 1;
            break;
        case GE:
            passed = (!state->cpsr.N == !state->cpsr.V);
            break;
        case LT:
            passed = (!state->cpsr.N != !state->cpsr.V);
            break;
        case GT:
            passed = (!state->cpsr.Z && !state->cpsr.N == !state->cpsr.V);
            break;
        case LE:
            passed = (state->cpsr.Z || !state->cpsr.N != !state->cpsr.V);
            break;
        case AL:
            passed = true;
            break;
        case NV:
            passed = false;
            break;
        default:
            logfatal("Unimplemented COND: %d", instr->parsed.cond)
    }
    return passed;
}


void tick(arm7tdmi_t* state, int ticks) {
    state->this_step_ticks += ticks;
}

arminstr_t next_arm_instr(arm7tdmi_t* state) {
    arminstr_t instr;
    instr.raw = state->pipeline[0];
    state->pipeline[0] = state->pipeline[1];
    state->pc += 4;
    state->pipeline[1] = state->read_word(state->pc);

    return instr;
}

thumbinstr_t next_thumb_instr(arm7tdmi_t* state) {
    thumbinstr_t instr;
    instr.raw = state->pipeline[0];
    state->pipeline[0] = state->pipeline[1];
    state->pc += 2;
    state->pipeline[1] = state->read_half(state->pc);

    return instr;
}

void set_sp(arm7tdmi_t* state, word newvalue) {
    switch (state->cpsr.mode) {
        case MODE_FIQ:
            state->sp_fiq = newvalue;
            break;
        case MODE_SUPERVISOR:
            state->sp_svc = newvalue;
            break;
        case MODE_ABORT:
            state->sp_abt = newvalue;
            break;
        case MODE_IRQ:
            state->sp_irq = newvalue;
            break;
        case MODE_UNDEFINED:
            state->sp_und = newvalue;
            break;
        default:
            state->sp = newvalue;
            break;
    }
}

word get_sp(arm7tdmi_t* state) {
    switch (state->cpsr.mode) {
        case MODE_FIQ:
            return state->sp_fiq;
        case MODE_SUPERVISOR:
            return state->sp_svc;
        case MODE_ABORT:
            return state->sp_abt;
        case MODE_IRQ:
            return state->sp_irq;
        case MODE_UNDEFINED:
            return state->sp_und;
        default:
            return state->sp;
    }
}

void set_lr(arm7tdmi_t* state, word newvalue) {
    switch (state->cpsr.mode) {
        case MODE_FIQ:
            state->lr_fiq = newvalue;
            break;
        case MODE_SUPERVISOR:
            state->lr_svc = newvalue;
            break;
        case MODE_ABORT:
            state->lr_abt = newvalue;
            break;
        case MODE_IRQ:
            state->lr_irq = newvalue;
            break;
        case MODE_UNDEFINED:
            state->lr_und = newvalue;
            break;
        default:
            state->lr = newvalue;
            break;
    }
}

word get_lr(arm7tdmi_t* state) {
    switch (state->cpsr.mode) {
        case MODE_FIQ:
            return state->lr_fiq;
        case MODE_SUPERVISOR:
            return state->lr_svc;
        case MODE_ABORT:
            return state->lr_abt;
        case MODE_IRQ:
            return state->lr_irq;
        case MODE_UNDEFINED:
            return state->lr_und;
        default:
            return state->lr;
    }
}

void set_register(arm7tdmi_t* state, word index, word newvalue) {
    logdebug("Set r%d to 0x%08X", index, newvalue)

    if (state->cpsr.mode == MODE_FIQ && index >= 8 && index <= 12) {
        state->highreg_fiq[index - 8] = newvalue;
    } else if (index < 13) {
        state->r[index] = newvalue;
    } else if (index == 13) {
        set_sp(state, newvalue);
    } else if (index == 14) {
        set_lr(state, newvalue);
    } else if (index == 15) {
        set_pc(state, newvalue);
    } else {
        logfatal("Attempted to write unknown register: r%d", index)
    }
}

word get_register(arm7tdmi_t* state, word index) {
    word value = 0;
    if (state->cpsr.mode == MODE_FIQ && index >= 8 && index <= 12) {
        value = state->highreg_fiq[index - 8];
    } else if (index < 13) {
        value = state->r[index];
    } else if (index == 13) {
        value = get_sp(state);
    } else if (index == 14) {
        value = get_lr(state);
    } else if (index == 15) {
        value = state->pc;
    } else {
        logfatal("Attempted to read unknown register: r%d", index)
    }

    return value;
}

#define cpsrflag(f, c) (f == 1?c:"-")

int arm_mode_step(arm7tdmi_t* state, arminstr_t* instr) {
    logdebug("cond: %d", instr->parsed.cond)
    if (check_cond(state, instr)) {
        arm_instr_type_t type = get_arm_instr_type(instr);
        switch (type) {
            case DATA_PROCESSING:
                data_processing(state, &instr->parsed.DATA_PROCESSING);
                break;
            case STATUS_TRANSFER:
                psr_transfer(state,
                             instr->parsed.DATA_PROCESSING.immediate,
                             instr->parsed.DATA_PROCESSING.opcode,
                             instr->parsed.DATA_PROCESSING.rn,
                             instr->parsed.DATA_PROCESSING.rd,
                             instr->parsed.DATA_PROCESSING.operand2);
                break;
            case MULTIPLY:
                multiply(state, &instr->parsed.MULTIPLY);
                break;
            case MULTIPLY_LONG:
                multiply_long(state, &instr->parsed.MULTIPLY_LONG);
                break;
            case SINGLE_DATA_SWAP:
                single_data_swap(state, &instr->parsed.SINGLE_DATA_SWAP);
                break;
            case BRANCH_EXCHANGE:
                branch_exchange(state, &instr->parsed.BRANCH_EXCHANGE);
                break;
            case HALFWORD_DT_RO:
                halfword_dt_ro(state,
                               instr->parsed.HALFWORD_DT_RO.p,
                               instr->parsed.HALFWORD_DT_RO.u,
                               instr->parsed.HALFWORD_DT_RO.w,
                               instr->parsed.HALFWORD_DT_RO.l,
                               instr->parsed.HALFWORD_DT_RO.rn,
                               instr->parsed.HALFWORD_DT_RO.rd,
                               instr->parsed.HALFWORD_DT_RO.s,
                               instr->parsed.HALFWORD_DT_RO.h,
                               instr->parsed.HALFWORD_DT_RO.rm);
                break;
            case HALFWORD_DT_IO: {
                byte offset = instr->parsed.HALFWORD_DT_IO.offset_low | (instr->parsed.HALFWORD_DT_IO.offset_high << 4u);
                halfword_dt_io(state,
                               instr->parsed.HALFWORD_DT_IO.p,
                               instr->parsed.HALFWORD_DT_IO.u,
                               instr->parsed.HALFWORD_DT_IO.w,
                               instr->parsed.HALFWORD_DT_IO.l,
                               instr->parsed.HALFWORD_DT_IO.rn,
                               instr->parsed.HALFWORD_DT_IO.rd,
                               offset,
                               instr->parsed.HALFWORD_DT_IO.s,
                               instr->parsed.HALFWORD_DT_IO.h);
                break;
            }
            case SINGLE_DATA_TRANSFER:
                single_data_transfer(state,
                                     instr->parsed.SINGLE_DATA_TRANSFER.offset,
                                     instr->parsed.SINGLE_DATA_TRANSFER.rd,
                                     instr->parsed.SINGLE_DATA_TRANSFER.rn,
                                     instr->parsed.SINGLE_DATA_TRANSFER.l,
                                     instr->parsed.SINGLE_DATA_TRANSFER.w,
                                     instr->parsed.SINGLE_DATA_TRANSFER.b,
                                     instr->parsed.SINGLE_DATA_TRANSFER.u,
                                     instr->parsed.SINGLE_DATA_TRANSFER.p,
                                     instr->parsed.SINGLE_DATA_TRANSFER.i);
                break;
            case UNDEFINED:
                logfatal("Unimplemented instruction type: UNDEFINED")
            case BLOCK_DATA_TRANSFER:
                block_data_transfer(state, &instr->parsed.BLOCK_DATA_TRANSFER);
                break;
            case BRANCH:
                branch(state, &instr->parsed.BRANCH);
                break;
            case COPROCESSOR_DATA_TRANSFER:
                logfatal("Unimplemented instruction type: COPROCESSOR_DATA_TRANSFER")
            case COPROCESSOR_DATA_OPERATION:
                logfatal("Unimplemented instruction type: COPROCESSOR_DATA_OPERATION")
            case COPROCESSOR_REGISTER_TRANSFER:
                logfatal("Unimplemented instruction type: COPROCESSOR_REGISTER_TRANSFER")
            case SOFTWARE_INTERRUPT:
                arm_software_interrupt(state, &instr->parsed.SOFTWARE_INTERRUPT);
                break;
            default:
                logfatal("Hit default case in arm_mode_step switch. This should never happen!")
        }
    }
    else { // Cond told us not to execute this instruction
        logdebug("Skipping instr because cond %d was not met.", instr->parsed.cond)
        tick(state, 1);
    }
    return state->this_step_ticks;
}

int thumb_mode_step(arm7tdmi_t* state, thumbinstr_t* instr) {
    thumb_instr_type_t type = get_thumb_instr_type(instr);

    switch (type) {
        case MOVE_SHIFTED_REGISTER:
            move_shifted_register(state, &instr->MOVE_SHIFTED_REGISTER);
            break;
        case ADD_SUBTRACT:
            add_subtract(state, &instr->ADD_SUBTRACT);
            break;
        case IMMEDIATE_OPERATIONS:
            immediate_operations(state, &instr->IMMEDIATE_OPERATIONS);
            break;
        case ALU_OPERATIONS:
            alu_operations(state, &instr->ALU_OPERATIONS);
            break;
        case HIGH_REGISTER_OPERATIONS:
            high_register_operations(state, &instr->HIGH_REGISTER_OPERATIONS);
            break;
        case PC_RELATIVE_LOAD:
            pc_relative_load(state, &instr->PC_RELATIVE_LOAD);
            break;
        case LOAD_STORE_RO:
            load_store_ro(state, &instr->LOAD_STORE_RO);
            break;
        case LOAD_STORE_BYTE_HALFWORD:
            load_store_byte_halfword(state, &instr->LOAD_STORE_BYTE_HALFWORD);
            break;
        case LOAD_STORE_IO:
            load_store_io(state, &instr->LOAD_STORE_IO);
            break;
        case LOAD_STORE_HALFWORD:
            load_store_halfword(state, &instr->LOAD_STORE_HALFWORD);
            break;
        case SP_RELATIVE_LOAD_STORE:
            sp_relative_load_store(state, &instr->SP_RELATIVE_LOAD_STORE);
            break;
        case LOAD_ADDRESS:
            load_address(state, &instr->LOAD_ADDRESS);
            break;
        case ADD_OFFSET_TO_STACK_POINTER:
            add_offset_to_stack_pointer(state, &instr->ADD_OFFSET_TO_STACK_POINTER);
            break;
        case PUSH_POP_REGISTERS:
            push_pop_registers(state, &instr->PUSH_POP_REGISTERS);
            break;
        case MULTIPLE_LOAD_STORE:
            multiple_load_store(state, &instr->MULTIPLE_LOAD_STORE);
            break;
        case CONDITIONAL_BRANCH:
            conditional_branch(state, &instr->CONDITIONAL_BRANCH);
            break;
        case THUMB_SOFTWARE_INTERRUPT:
            thumb_software_interrupt(state, &instr->THUMB_SOFTWARE_INTERRUPT);
            break;
        case UNCONDITIONAL_BRANCH:
            unconditional_branch(state, &instr->UNCONDITIONAL_BRANCH);
            break;
        case LONG_BRANCH_LINK:
            long_branch_link(state, &instr->LONG_BRANCH_LINK);
            break;
        case THUMB_UNDEFINED:
            logfatal("Unimplemented THUMB mode instruction type: THUMB_UNDEFINED")
        default:
            logfatal("Hit default case in arm_mode_step switch. This should never happen!")
    }

    return state->this_step_ticks;
}

void handle_irq(arm7tdmi_t* state) {
    logwarn("IRQ!")
    status_register_t cpsr = state->cpsr;
    state->halt = false;
    state->cpsr.mode = MODE_IRQ;
    set_spsr(state, cpsr.raw);
    state->cpsr.thumb = 0;
    state->cpsr.disable_irq = 1;
    state->lr_irq = state->pc - (cpsr.thumb ? 2 : 4) + 4;
    set_pc(state, 0x18); // IRQ handler
}

int arm7tdmi_step(arm7tdmi_t* state) {
    if (state->irq && !state->cpsr.disable_irq) {
        handle_irq(state);
    }



    dbg_tick(INSTRUCTION);

    state->this_step_ticks = 0;
    logdebug("r0:  %08X   r1: %08X   r2: %08X   r3: %08X", get_register(state, 0), get_register(state, 1), get_register(state, 2), get_register(state, 3))
    logdebug("r4:  %08X   r5: %08X   r6: %08X   r7: %08X", get_register(state, 4), get_register(state, 5), get_register(state, 6), get_register(state, 7))
    logdebug("r8:  %08X   r9: %08X  r10: %08X  r11: %08X", get_register(state, 8), get_register(state, 9), get_register(state, 10), get_register(state, 11))
    logdebug("r12: %08X  r13: %08X  r14: %08X  r15: %08X", get_register(state, 12), get_register(state, 13), get_register(state, 14), get_register(state, 15))
    logdebug("cpsr: %08X [%s%s%s%s%s%s%s]", state->cpsr.raw, cpsrflag(state->cpsr.N, "N"), cpsrflag(state->cpsr.Z, "Z"),
             cpsrflag(state->cpsr.C, "C"), cpsrflag(state->cpsr.V, "V"), cpsrflag(state->cpsr.disable_irq, "I"),
             cpsrflag(state->cpsr.disable_fiq, "F"), cpsrflag(state->cpsr.thumb, "T"))

     int cycles;
     if (state->cpsr.thumb) {
         thumbinstr_t instr = next_thumb_instr(state);
         state->instr = instr.raw;
         word adjusted_pc = state->pc - 4;
         if (log_get_verbosity() >= LOG_VERBOSITY_INFO) {
             disassemble_thumb(adjusted_pc, instr.raw, (char *) &state->disassembled, sizeof(state->disassembled));
             loginfo("[THM]  [%s] 0x%08X: %s", MODE_NAMES[state->cpsr.mode], adjusted_pc, state->disassembled)
         }
         cycles = thumb_mode_step(state, &instr);
     } else {
         arminstr_t instr = next_arm_instr(state);
         state->instr = instr.raw;
         word adjusted_pc = state->pc - 8;
         if (log_get_verbosity() >= LOG_VERBOSITY_INFO) {
             disassemble_arm(adjusted_pc, instr.raw, (char *) &state->disassembled, sizeof(state->disassembled));
             loginfo("[ARM] [%s] 0x%08X: %s", MODE_NAMES[state->cpsr.mode], adjusted_pc, state->disassembled)
         }
         cycles = arm_mode_step(state, &instr);
     }

    // Assume a step of 0 cycles is an unknown number of cycles. Consider it 1 cycle
     return cycles == 0 ? 1 : cycles;
}

status_register_t* get_psr(arm7tdmi_t* state) {
    return &state->cpsr;
}

void set_psr(arm7tdmi_t* state, word value) {
    state->cpsr.raw = value;
}

status_register_t* get_spsr(arm7tdmi_t* state) {
    switch (state->cpsr.mode) {
        case MODE_FIQ:
            return &state->spsr_fiq;
        case MODE_SUPERVISOR:
            return &state->spsr_svc;
        case MODE_ABORT:
            return &state->spsr_abt;
        case MODE_IRQ:
            return &state->spsr_irq;
        case MODE_UNDEFINED:
            return &state->spsr_und;
        default:
            return &state->spsr;
    }
}

void set_spsr(arm7tdmi_t* state, word value) {
    status_register_t* spsr = get_spsr(state);
    spsr->raw = value;
}

void set_flags_nz(arm7tdmi_t* state, word newvalue) {
    status_register_t* psr = get_psr(state);
    psr->Z = newvalue == 0;
    psr->N = newvalue >> 31u;
}

void set_flags_add(arm7tdmi_t* state, uint64_t op1, uint64_t op2) {
    uint32_t result = op1 + op2;
    state->cpsr.C = op1 + op2 > 0xFFFFFFFF;
    state->cpsr.V = ((op1 ^ result) & (~op1 ^ op2)) >> 31u;
}

void set_flags_sub(arm7tdmi_t* state, word op1, word op2, word result) {
    state->cpsr.C = op2 <= op1;
    state->cpsr.V = ((op1 ^ op2) & (~op2 ^ result)) >> 31u;
}

void set_flags_sbc(arm7tdmi_t* state, word op1, word op2, uint64_t tmp, word result) {
    state->cpsr.C = tmp <= op1;
    state->cpsr.V = ((op1 ^ op2) & (~op2 ^ result)) >> 31u;
}

void skip_bios(arm7tdmi_t* state) {
    set_register(state, 0, 0x08000000);
    set_register(state, 1, 0x000000EA);
    set_register(state, 2, 0x00000000);
    set_register(state, 3, 0x00000000);
    set_register(state, 4, 0x00000000);
    set_register(state, 5, 0x00000000);
    set_register(state, 6, 0x00000000);
    set_register(state, 7, 0x00000000);
    set_register(state, 8, 0x00000000);
    set_register(state, 9, 0x00000000);
    set_register(state, 10, 0x00000000);
    set_register(state, 11, 0x00000000);
    set_register(state, 12, 0x00000000);
    set_register(state, REG_SP, 0x03007F00);
    state->sp_irq = 0x3007FA0;
    state->sp_svc = 0x3007FE0;
    set_register(state, REG_LR, 0x00000000);

    set_pc(state, 0x08000000);
    state->cpsr.raw = 0x6000001F;
}
