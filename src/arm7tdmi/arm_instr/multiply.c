#include "multiply.h"
#include "../../common/log.h"

void multiply(arm7tdmi_t* state, multiply_t* instr) {
    unimplemented(instr->rm == instr->rd, "rm must not be the same as rd!")

    unimplemented(instr->rd == 15, "rd must not be 15!")
    unimplemented(instr->rn == 15, "rn must not be 15!")
    unimplemented(instr->rs == 15, "rs must not be 15!")
    unimplemented(instr->rm == 15, "rm must not be 15!")

    uint64_t result = get_register(state, instr->rm);
    result *= get_register(state, instr->rs);

    if (instr->a) {
        result += get_register(state, instr->rn);
    }

    word wordresult = result & 0xFFFFFFFF;

    unimplemented(instr->s, "Status codes")

    set_register(state, instr->rd, wordresult);
}

uint64_t sign_extend(uint64_t v, int old, int new) {
    unimplemented(new < old, "Can't downsize signed values with this function!")

    uint64_t mask = v & (1 << (old - 1));
    bool s = mask > 0;
    if (s) {
        return (v ^ mask) - mask;
    }
    else {
        return v; // No sign bit set, don't need to sign extend
    }
}

void multiply_long(arm7tdmi_t* state, multiply_long_t* instr) {
    uint64_t rmdata = get_register(state, instr->rm);
    uint64_t rsdata = get_register(state, instr->rs);
    uint64_t result;
    if (instr->u) {
        rmdata = sign_extend(rmdata, 32, 64);
        rsdata = sign_extend(rsdata, 32, 64);
    }

    result = rmdata * rsdata;

    if (instr->a) { // accumulate (add on to what's already set)
        uint64_t existing = get_register(state, instr->rdhi);
        existing <<= 32;
        existing |= get_register(state, instr->rdlo) & 0xFFFFFFFF;
        result += existing;
    }

    if (instr->s) {
        status_register_t* psr = get_psr(state);
        psr->Z = result == 0;
        psr->N = result >> 63;
    }

    word high = (result >> 32u) & 0xFFFFFFFF;
    word low = result & 0xFFFFFFFF;

    set_register(state, instr->rdlo, low);
    set_register(state, instr->rdhi, high);
}