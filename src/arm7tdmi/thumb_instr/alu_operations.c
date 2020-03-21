#include "alu_operations.h"
#include "../../common/log.h"

void alu_operations(arm7tdmi_t* state, alu_operations_t* instr) {
    switch (instr->opcode) {
        case 0x0: { // AND: Rd = Rd AND Rs
            word newvalue = get_register(state, instr->rd) & get_register(state, instr->rs);
            set_flags_nz(state, newvalue);
            set_register(state, instr->rd, newvalue);
            break;
        }
        case 0x1: { // EOR: Rd = Rd EOR Rs
            logfatal("EOR")
        }
        case 0x2: { // LSL: Rd = Rd << Rs
            logfatal("LSL")
        }
        case 0x3: { // LSR: Rd = Rd >> Rs
            logfatal("LSR")
        }
        case 0x4: { // ASR: Rd = Rd ASR Rs
            logfatal("ASR")
        }
        case 0x5: { // ADC: Rd = Rd + Rs + C
            logfatal("ADC")
        }
        case 0x6: { // SBC: Rd = Rd - Rs - (~C)
            logfatal("SBC")
        }
        case 0x7: { // ROR: Rd = Rd ROR Rs
            logfatal("ROR")
        }
        case 0x8: { // TST: Set condition codes on Rd AND Rs
            logfatal("TST")
        }
        case 0x9: { // NEG: Rd = -Rs
            logfatal("NEG")
        }
        case 0xA: { // CMP: Set condition codes on Rd - Rs
            word rddata = get_register(state, instr->rd);
            word rsdata = get_register(state, instr->rs);
            word result = rddata - rsdata;
            set_flags_nz(state, result);
            set_flags_sub(state, rddata, rsdata, result);
            break;
        }
        case 0xB: { // CMN: Set condition codes on Rd + Rs
            logfatal("CMN")
        }
        case 0xC: { // ORR: Rd = Rd | Rs
            word newvalue = get_register(state, instr->rd) | get_register(state, instr->rs);
            set_flags_nz(state, newvalue);
            set_register(state, instr->rd, newvalue);
            break;
        }
        case 0xD: { // MUL: Rd = Rd * Rs
            logfatal("MUL")
        }
        case 0xE: { // BIC: Rd = Rd & ~Rs
            logfatal("BIC")
        }
        case 0xF: { // MVN: Rd = ~Rs
            logfatal("MVN")
        }
        default:
            logfatal("Unknown ALU operations opcode: %d", instr->opcode)
    }
}
