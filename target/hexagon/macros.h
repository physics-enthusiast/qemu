/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_MACROS_H
#define HEXAGON_MACROS_H

#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "cpu.h"
#include "hex_regs.h"
#include "reg_fields.h"

#ifdef QEMU_GENERATE
#define READ_REG(dest, NUM)              gen_read_reg(dest, NUM)
#define READ_PREG(dest, NUM)             gen_read_preg(dest, (NUM))
#else
#define READ_REG(NUM)                    (env->gpr[(NUM)])
#define READ_PREG(NUM)                   (env->pred[NUM])

#define WRITE_RREG(NUM, VAL)             log_reg_write(env, NUM, VAL, slot)
#define WRITE_PREG(NUM, VAL)             log_pred_write(env, NUM, VAL)
#endif

#define PCALIGN 4
#define PCALIGN_MASK (PCALIGN - 1)

#define GET_FIELD(FIELD, REGIN) \
    fEXTRACTU_BITS(REGIN, reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)

#define GET_USR_FIELD(FIELD) \
    fEXTRACTU_BITS(env->gpr[HEX_REG_USR], reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)

#define SET_USR_FIELD(FIELD, VAL) \
    fINSERT_BITS(env->gpr[HEX_REG_USR], reg_field_info[FIELD].width, \
                 reg_field_info[FIELD].offset, (VAL))

#ifdef QEMU_GENERATE
/*
 * Section 5.5 of the Hexagon V67 Programmer's Reference Manual
 *
 * Slot 1 store with slot 0 load
 * A slot 1 store operation with a slot 0 load operation can appear in a packet.
 * The packet attribute :mem_noshuf inhibits the instruction reordering that
 * would otherwise be done by the assembler. For example:
 *     {
 *         memw(R5) = R2 // slot 1 store
 *         R3 = memh(R6) // slot 0 load
 *     }:mem_noshuf
 * Unlike most packetized operations, these memory operations are not executed
 * in parallel (Section 3.3.1). Instead, the store instruction in Slot 1
 * effectively executes first, followed by the load instruction in Slot 0. If
 * the addresses of the two operations are overlapping, the load will receive
 * the newly stored data. This feature is supported in processor versions
 * V65 or greater.
 *
 *
 * For qemu, we look for a load in slot 0 when there is  a store in slot 1
 * in the same packet.  When we see this, we call a helper that merges the
 * bytes from the store buffer with the value loaded from memory.
 */
#define CHECK_NOSHUF(DST, VA, SZ, SIGN) \
    do { \
        if (insn->slot == 0 && pkt->pkt_has_store_s1) { \
            gen_helper_merge_inflight_store##SZ##SIGN(DST, cpu_env, VA, DST); \
        } \
    } while (0)

#define MEM_LOAD1s(DST, VA) \
    do { \
        tcg_gen_qemu_ld8s(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 1, s); \
    } while (0)
#define MEM_LOAD1u(DST, VA) \
    do { \
        tcg_gen_qemu_ld8u(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 1, u); \
    } while (0)
#define MEM_LOAD2s(DST, VA) \
    do { \
        tcg_gen_qemu_ld16s(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 2, s); \
    } while (0)
#define MEM_LOAD2u(DST, VA) \
    do { \
        tcg_gen_qemu_ld16u(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 2, u); \
    } while (0)
#define MEM_LOAD4s(DST, VA) \
    do { \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 4, s); \
    } while (0)
#define MEM_LOAD4u(DST, VA) \
    do { \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 4, u); \
    } while (0)
#define MEM_LOAD8u(DST, VA) \
    do { \
        tcg_gen_qemu_ld64(DST, VA, ctx->mem_idx); \
        CHECK_NOSHUF(DST, VA, 8, u); \
    } while (0)
#else
#define MEM_LOAD1s(VA) ((int8_t)mem_load1(env, slot, VA))
#define MEM_LOAD1u(VA) ((uint8_t)mem_load1(env, slot, VA))
#define MEM_LOAD2s(VA) ((int16_t)mem_load2(env, slot, VA))
#define MEM_LOAD2u(VA) ((uint16_t)mem_load2(env, slot, VA))
#define MEM_LOAD4s(VA) ((int32_t)mem_load4(env, slot, VA))
#define MEM_LOAD4u(VA) ((uint32_t)mem_load4(env, slot, VA))
#define MEM_LOAD8s(VA) ((int64_t)mem_load8(env, slot, VA))
#define MEM_LOAD8u(VA) ((uint64_t)mem_load8(env, slot, VA))

#define MEM_STORE1(VA, DATA, SLOT) log_store32(env, VA, DATA, 1, SLOT)
#define MEM_STORE2(VA, DATA, SLOT) log_store32(env, VA, DATA, 2, SLOT)
#define MEM_STORE4(VA, DATA, SLOT) log_store32(env, VA, DATA, 4, SLOT)
#define MEM_STORE8(VA, DATA, SLOT) log_store64(env, VA, DATA, 8, SLOT)
#endif

#define CANCEL cancel_slot(env, slot)

#define LOAD_CANCEL(EA) do { CANCEL; } while (0)

#ifdef QEMU_GENERATE
static inline void gen_pred_cancel(TCGv pred, int slot_num)
 {
    TCGv slot_mask = tcg_const_tl(1 << slot_num);
    TCGv tmp = tcg_temp_new();
    TCGv zero = tcg_const_tl(0);
    TCGv one = tcg_const_tl(1);
    tcg_gen_or_tl(slot_mask, hex_slot_cancelled, slot_mask);
    tcg_gen_andi_tl(tmp, pred, 1);
    tcg_gen_movcond_tl(TCG_COND_EQ, hex_slot_cancelled, tmp, zero,
                       slot_mask, hex_slot_cancelled);
    tcg_temp_free(slot_mask);
    tcg_temp_free(tmp);
    tcg_temp_free(zero);
    tcg_temp_free(one);
}
#define PRED_LOAD_CANCEL(PRED, EA) \
    gen_pred_cancel(PRED, insn->is_endloop ? 4 : insn->slot)
#endif

#define STORE_CANCEL(EA) { env->slot_cancelled |= (1 << slot); }

#define fMAX(A, B) (((A) > (B)) ? (A) : (B))

#define fMIN(A, B) (((A) < (B)) ? (A) : (B))

#define fABS(A) (((A) < 0) ? (-(A)) : (A))
#define fINSERT_BITS(REG, WIDTH, OFFSET, INVAL) \
    REG = ((WIDTH) ? deposit64(REG, (OFFSET), (WIDTH), (INVAL)) : REG)
#define fEXTRACTU_BITS(INREG, WIDTH, OFFSET) \
    ((WIDTH) ? extract64((INREG), (OFFSET), (WIDTH)) : 0LL)
#define fEXTRACTU_BIDIR(INREG, WIDTH, OFFSET) \
    (fZXTN(WIDTH, 32, fBIDIR_LSHIFTR((INREG), (OFFSET), 4_8)))
#define fEXTRACTU_RANGE(INREG, HIBIT, LOWBIT) \
    (((HIBIT) - (LOWBIT) + 1) ? \
        extract64((INREG), (LOWBIT), ((HIBIT) - (LOWBIT) + 1)) : \
        0LL)

#define f8BITSOF(VAL) ((VAL) ? 0xff : 0x00)

#ifdef QEMU_GENERATE
#define fLSBOLD(VAL) tcg_gen_andi_tl(LSB, (VAL), 1)
#else
#define fLSBOLD(VAL)  ((VAL) & 1)
#endif

#ifdef QEMU_GENERATE
#define fLSBNEW(PVAL)   tcg_gen_mov_tl(LSB, (PVAL))
#define fLSBNEW0        fLSBNEW(0)
#define fLSBNEW1        fLSBNEW(1)
#else
#define fLSBNEW(PVAL)   (PVAL)
#define fLSBNEW0        new_pred_value(env, 0)
#define fLSBNEW1        new_pred_value(env, 1)
#endif

#ifdef QEMU_GENERATE
static inline void gen_logical_not(TCGv dest, TCGv src)
{
    TCGv one = tcg_const_tl(1);
    TCGv zero = tcg_const_tl(0);

    tcg_gen_movcond_tl(TCG_COND_NE, dest, src, zero, zero, one);

    tcg_temp_free(one);
    tcg_temp_free(zero);
}
#define fLSBOLDNOT(VAL) \
    do { \
        tcg_gen_andi_tl(LSB, (VAL), 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
    } while (0)
#define fLSBNEWNOT(PNUM) \
    gen_logical_not(LSB, (PNUM))
#else
#define fLSBNEWNOT(PNUM) (!fLSBNEW(PNUM))
#define fLSBOLDNOT(VAL) (!fLSBOLD(VAL))
#define fLSBNEW0NOT (!fLSBNEW0)
#define fLSBNEW1NOT (!fLSBNEW1)
#endif

#define fNEWREG(RNUM) ((int32_t)(env->new_value[(RNUM)]))

#define fNEWREG_ST(RNUM) (env->new_value[(RNUM)])

#define fSATUVALN(N, VAL) \
    ({ \
        fSET_OVERFLOW(); \
        ((VAL) < 0) ? 0 : ((1LL << (N)) - 1); \
    })
#define fSATVALN(N, VAL) \
    ({ \
        fSET_OVERFLOW(); \
        ((VAL) < 0) ? (-(1LL << ((N) - 1))) : ((1LL << ((N) - 1)) - 1); \
    })
#define fZXTN(N, M, VAL) ((N) ? extract64((VAL), 0, (N)) : 0LL)
#define fSXTN(N, M, VAL) ((N) ? sextract64((VAL), 0, (N)) : 0LL)
#define fSATN(N, VAL) \
    ((fSXTN(N, 64, VAL) == (VAL)) ? (VAL) : fSATVALN(N, VAL))
#define fADDSAT64(DST, A, B) \
    do { \
        uint64_t __a = fCAST8u(A); \
        uint64_t __b = fCAST8u(B); \
        uint64_t __sum = __a + __b; \
        uint64_t __xor = __a ^ __b; \
        const uint64_t __mask = 0x8000000000000000ULL; \
        if (__xor & __mask) { \
            DST = __sum; \
        } \
        else if ((__a ^ __sum) & __mask) { \
            if (__sum & __mask) { \
                DST = 0x7FFFFFFFFFFFFFFFLL; \
                fSET_OVERFLOW(); \
            } else { \
                DST = 0x8000000000000000LL; \
                fSET_OVERFLOW(); \
            } \
        } else { \
            DST = __sum; \
        } \
    } while (0)
#define fSATUN(N, VAL) \
    ((fZXTN(N, 64, VAL) == (VAL)) ? (VAL) : fSATUVALN(N, VAL))
#define fSATH(VAL) (fSATN(16, VAL))
#define fSATUH(VAL) (fSATUN(16, VAL))
#define fSATUB(VAL) (fSATUN(8, VAL))
#define fSATB(VAL) (fSATN(8, VAL))
#define fIMMEXT(IMM) (IMM = IMM)
#define fMUST_IMMEXT(IMM) fIMMEXT(IMM)

#define fPCALIGN(IMM) IMM = (IMM & ~PCALIGN_MASK)

#define fREAD_LR() (READ_REG(HEX_REG_LR))

#define fWRITE_LR(A) WRITE_RREG(HEX_REG_LR, A)
#define fWRITE_FP(A) WRITE_RREG(HEX_REG_FP, A)
#define fWRITE_SP(A) WRITE_RREG(HEX_REG_SP, A)

#define fREAD_SP() (READ_REG(HEX_REG_SP))
#define fREAD_LC0 (READ_REG(HEX_REG_LC0))
#define fREAD_LC1 (READ_REG(HEX_REG_LC1))
#define fREAD_SA0 (READ_REG(HEX_REG_SA0))
#define fREAD_SA1 (READ_REG(HEX_REG_SA1))
#define fREAD_FP() (READ_REG(HEX_REG_FP))
#ifdef FIXME
/* Figure out how to get insn->extension_valid to helper */
#define fREAD_GP() \
    (insn->extension_valid ? 0 : READ_REG(HEX_REG_GP))
#else
#define fREAD_GP() READ_REG(HEX_REG_GP)
#endif
#define fREAD_PC() (READ_REG(HEX_REG_PC))

#define fREAD_NPC() (env->next_PC & (0xfffffffe))

#define fREAD_P0() (READ_PREG(0))
#define fREAD_P3() (READ_PREG(3))

#define fCHECK_PCALIGN(A)

#define fWRITE_NPC(A) write_new_pc(env, A)

#define fBRANCH(LOC, TYPE)          fWRITE_NPC(LOC)
#define fJUMPR(REGNO, TARGET, TYPE) fBRANCH(TARGET, COF_TYPE_JUMPR)
#define fHINTJR(TARGET) { /* Not modelled in qemu */}
#define fCALL(A) \
    do { \
        fWRITE_LR(fREAD_NPC()); \
        fBRANCH(A, COF_TYPE_CALL); \
    } while (0)
#define fCALLR(A) \
    do { \
        fWRITE_LR(fREAD_NPC()); \
        fBRANCH(A, COF_TYPE_CALLR); \
    } while (0)
#define fWRITE_LOOP_REGS0(START, COUNT) \
    do { \
        WRITE_RREG(HEX_REG_LC0, COUNT);  \
        WRITE_RREG(HEX_REG_SA0, START); \
    } while (0)
#define fWRITE_LOOP_REGS1(START, COUNT) \
    do { \
        WRITE_RREG(HEX_REG_LC1, COUNT);  \
        WRITE_RREG(HEX_REG_SA1, START);\
    } while (0)
#define fWRITE_LC0(VAL) WRITE_RREG(HEX_REG_LC0, VAL)
#define fWRITE_LC1(VAL) WRITE_RREG(HEX_REG_LC1, VAL)

#define fCARRY_FROM_ADD(A, B, C) carry_from_add64(A, B, C)

#define fSET_OVERFLOW() SET_USR_FIELD(USR_OVF, 1)
#define fSET_LPCFG(VAL) SET_USR_FIELD(USR_LPCFG, (VAL))
#define fGET_LPCFG (GET_USR_FIELD(USR_LPCFG))
#define fWRITE_P0(VAL) WRITE_PREG(0, VAL)
#define fWRITE_P1(VAL) WRITE_PREG(1, VAL)
#define fWRITE_P2(VAL) WRITE_PREG(2, VAL)
#define fWRITE_P3(VAL) WRITE_PREG(3, VAL)
#define fPART1(WORK) if (part1) { WORK; return; }
#define fCAST4u(A) ((uint32_t)(A))
#define fCAST4s(A) ((int32_t)(A))
#define fCAST8u(A) ((uint64_t)(A))
#define fCAST8s(A) ((int64_t)(A))
#define fCAST4_4s(A) ((int32_t)(A))
#define fCAST4_4u(A) ((uint32_t)(A))
#define fCAST4_8s(A) ((int64_t)((int32_t)(A)))
#define fCAST4_8u(A) ((uint64_t)((uint32_t)(A)))
#define fCAST8_8s(A) ((int64_t)(A))
#define fCAST8_8u(A) ((uint64_t)(A))
#define fCAST2_8s(A) ((int64_t)((int16_t)(A)))
#define fCAST2_8u(A) ((uint64_t)((uint16_t)(A)))
#define fZE8_16(A) ((int16_t)((uint8_t)(A)))
#define fSE8_16(A) ((int16_t)((int8_t)(A)))
#define fSE16_32(A) ((int32_t)((int16_t)(A)))
#define fZE16_32(A) ((uint32_t)((uint16_t)(A)))
#define fSE32_64(A) ((int64_t)((int32_t)(A)))
#define fZE32_64(A) ((uint64_t)((uint32_t)(A)))
#define fSE8_32(A) ((int32_t)((int8_t)(A)))
#define fZE8_32(A) ((int32_t)((uint8_t)(A)))
#define fMPY8UU(A, B) (int)(fZE8_16(A) * fZE8_16(B))
#define fMPY8US(A, B) (int)(fZE8_16(A) * fSE8_16(B))
#define fMPY8SU(A, B) (int)(fSE8_16(A) * fZE8_16(B))
#define fMPY8SS(A, B) (int)((short)(A) * (short)(B))
#define fMPY16SS(A, B) fSE32_64(fSE16_32(A) * fSE16_32(B))
#define fMPY16UU(A, B) fZE32_64(fZE16_32(A) * fZE16_32(B))
#define fMPY16SU(A, B) fSE32_64(fSE16_32(A) * fZE16_32(B))
#define fMPY16US(A, B) fMPY16SU(B, A)
#define fMPY32SS(A, B) (fSE32_64(A) * fSE32_64(B))
#define fMPY32UU(A, B) (fZE32_64(A) * fZE32_64(B))
#define fMPY32SU(A, B) (fSE32_64(A) * fZE32_64(B))
#define fMPY3216SS(A, B) (fSE32_64(A) * fSXTN(16, 64, B))
#define fMPY3216SU(A, B) (fSE32_64(A) * fZXTN(16, 64, B))
#define fROUND(A) (A + 0x8000)
#define fCLIP(DST, SRC, U) \
    do { \
        int32_t maxv = (1 << U) - 1; \
        int32_t minv = -(1 << U); \
        DST = fMIN(maxv, fMAX(SRC, minv)); \
    } while (0)
#define fCRND(A) ((((A) & 0x3) == 0x3) ? ((A) + 1) : ((A)))
#define fRNDN(A, N) ((((N) == 0) ? (A) : (((fSE32_64(A)) + (1 << ((N) - 1))))))
#define fCRNDN(A, N) (conv_round(A, N))
#define fADD128(A, B) (add128(A, B))
#define fSUB128(A, B) (sub128(A, B))
#define fSHIFTR128(A, B) (shiftr128(A, B))
#define fSHIFTL128(A, B) (shiftl128(A, B))
#define fAND128(A, B) (and128(A, B))
#define fCAST8S_16S(A) (cast8s_to_16s(A))
#define fCAST16S_8S(A) (cast16s_to_8s(A))

#define fEA_RI(REG, IMM) \
    do { \
        EA = REG + IMM; \
    } while (0)
#define fEA_RRs(REG, REG2, SCALE) \
    do { \
        EA = REG + (REG2 << SCALE); \
    } while (0)
#define fEA_IRs(IMM, REG, SCALE) \
    do { \
        EA = IMM + (REG << SCALE); \
    } while (0)

#ifdef QEMU_GENERATE
#define fEA_IMM(IMM) tcg_gen_movi_tl(EA, (IMM))
#define fEA_REG(REG) tcg_gen_mov_tl(EA, REG)
#define fPM_I(REG, IMM)     tcg_gen_addi_tl(REG, REG, (IMM))
#define fPM_M(REG, MVAL)    tcg_gen_add_tl(REG, REG, MVAL)
#else
#define fEA_IMM(IMM)        do { EA = (IMM); } while (0)
#define fEA_REG(REG)        do { EA = (REG); } while (0)
#define fEA_GPI(IMM)        do { EA = fREAD_GP() + (IMM); } while (0)
#define fPM_I(REG, IMM)     do { REG = REG + (IMM); } while (0)
#define fPM_M(REG, MVAL)    do { REG = REG + (MVAL); } while (0)
#endif
#define fSCALE(N, A) (((int64_t)(A)) << N)
#define fSATW(A) fSATN(32, ((long long)A))
#define fSAT(A) fSATN(32, (A))
#define fSAT_ORIG_SHL(A, ORIG_REG) \
    ((((int32_t)((fSAT(A)) ^ ((int32_t)(ORIG_REG)))) < 0) \
        ? fSATVALN(32, ((int32_t)(ORIG_REG))) \
        : ((((ORIG_REG) > 0) && ((A) == 0)) ? fSATVALN(32, (ORIG_REG)) \
                                            : fSAT(A)))
#define fPASS(A) A
#define fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE(SRC) >> ((-(SHAMT)) - 1)) >> 1) \
                   : (fCAST##REGSTYPE(SRC) << (SHAMT)))
#define fBIDIR_ASHIFTL(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE##s)
#define fBIDIR_LSHIFTL(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE##u)
#define fBIDIR_ASHIFTL_SAT(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE##s(SRC) >> ((-(SHAMT)) - 1)) >> 1) \
                   : fSAT_ORIG_SHL(fCAST##REGSTYPE##s(SRC) << (SHAMT), (SRC)))
#define fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE(SRC) << ((-(SHAMT)) - 1)) << 1) \
                   : (fCAST##REGSTYPE(SRC) >> (SHAMT)))
#define fBIDIR_ASHIFTR(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE##s)
#define fBIDIR_LSHIFTR(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE##u)
#define fBIDIR_ASHIFTR_SAT(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? fSAT_ORIG_SHL((fCAST##REGSTYPE##s(SRC) \
                        << ((-(SHAMT)) - 1)) << 1, (SRC)) \
                   : (fCAST##REGSTYPE##s(SRC) >> (SHAMT)))
#define fASHIFTR(SRC, SHAMT, REGSTYPE) (fCAST##REGSTYPE##s(SRC) >> (SHAMT))
#define fLSHIFTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) >= 64) ? 0 : (fCAST##REGSTYPE##u(SRC) >> (SHAMT)))
#define fROTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) == 0) ? (SRC) : ((fCAST##REGSTYPE##u(SRC) << (SHAMT)) | \
                              ((fCAST##REGSTYPE##u(SRC) >> \
                                 ((sizeof(SRC) * 8) - (SHAMT))))))
#define fROTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) == 0) ? (SRC) : ((fCAST##REGSTYPE##u(SRC) >> (SHAMT)) | \
                              ((fCAST##REGSTYPE##u(SRC) << \
                                 ((sizeof(SRC) * 8) - (SHAMT))))))
#define fASHIFTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) >= 64) ? 0 : (fCAST##REGSTYPE##s(SRC) << (SHAMT)))
#define fFLOAT(A) \
    ({ union { float f; uint32_t i; } _fipun; _fipun.i = (A); _fipun.f; })
#define fUNFLOAT(A) \
    ({ union { float f; uint32_t i; } _fipun; \
     _fipun.f = (A); isnan(_fipun.f) ? 0xFFFFFFFFU : _fipun.i; })
#define fSFNANVAL() 0xffffffff
#define fSFINFVAL(A) (((A) & 0x80000000) | 0x7f800000)
#define fSFONEVAL(A) (((A) & 0x80000000) | fUNFLOAT(1.0))
#define fCHECKSFNAN(DST, A) \
    do { \
        if (isnan(fFLOAT(A))) { \
            if ((fGETBIT(22, A)) == 0) { \
                fRAISEFLAGS(FE_INVALID); \
            } \
            DST = fSFNANVAL(); \
        } \
    } while (0)
#define fCHECKSFNAN3(DST, A, B, C) \
    do { \
        fCHECKSFNAN(DST, A); \
        fCHECKSFNAN(DST, B); \
        fCHECKSFNAN(DST, C); \
    } while (0)
#define fSF_BIAS() 127
#define fSF_MANTBITS() 23
#define fSF_MUL_POW2(A, B) \
    (fUNFLOAT(fFLOAT(A) * fFLOAT((fSF_BIAS() + (B)) << fSF_MANTBITS())))
#define fSF_GETEXP(A) (((A) >> fSF_MANTBITS()) & 0xff)
#define fSF_MAXEXP() (254)
#define fSF_RECIP_COMMON(N, D, O, A) arch_sf_recip_common(&N, &D, &O, &A)
#define fSF_INVSQRT_COMMON(N, O, A) arch_sf_invsqrt_common(&N, &O, &A)
#define fFMAFX(A, B, C, ADJ) internal_fmafx(A, B, C, fSXTN(8, 64, ADJ))
#define fFMAF(A, B, C) internal_fmafx(A, B, C, 0)
#define fSFMPY(A, B) internal_mpyf(A, B)
#define fMAKESF(SIGN, EXP, MANT) \
    ((((SIGN) & 1) << 31) | \
     (((EXP) & 0xff) << fSF_MANTBITS()) | \
     ((MANT) & ((1 << fSF_MANTBITS()) - 1)))
#define fDOUBLE(A) \
    ({ union { double f; uint64_t i; } _fipun; _fipun.i = (A); _fipun.f; })
#define fUNDOUBLE(A) \
    ({ union { double f; uint64_t i; } _fipun; \
     _fipun.f = (A); \
     isnan(_fipun.f) ? 0xFFFFFFFFFFFFFFFFULL : _fipun.i; })
#define fDFNANVAL() 0xffffffffffffffffULL
#define fDF_ISNORMAL(X) (fpclassify(fDOUBLE(X)) == FP_NORMAL)
#define fDF_ISDENORM(X) (fpclassify(fDOUBLE(X)) == FP_SUBNORMAL)
#define fDF_ISBIG(X) (fDF_GETEXP(X) >= 512)
#define fDF_MANTBITS() 52
#define fDF_GETEXP(A) (((A) >> fDF_MANTBITS()) & 0x7ff)
#define fFMA(A, B, C) internal_fma(A, B, C)
#define fDF_MPY_HH(A, B, ACC) internal_mpyhh(A, B, ACC)

#ifdef QEMU_GENERATE
/* These will be needed if we write any FP instructions with TCG */
#define fFPOP_START()      /* nothing */
#define fFPOP_END()        /* nothing */
#else
#define fFPOP_START() arch_fpop_start(env)
#define fFPOP_END() arch_fpop_end(env)
#endif

#define fFPSETROUND_NEAREST() fesetround(FE_TONEAREST)
#define fFPSETROUND_CHOP() fesetround(FE_TOWARDZERO)
#define fFPCANCELFLAGS() feclearexcept(FE_ALL_EXCEPT)
#define fISINFPROD(A, B) \
    ((isinf(A) && isinf(B)) || \
     (isinf(A) && isfinite(B) && ((B) != 0.0)) || \
     (isinf(B) && isfinite(A) && ((A) != 0.0)))
#define fISZEROPROD(A, B) \
    ((((A) == 0.0) && isfinite(B)) || (((B) == 0.0) && isfinite(A)))
#define fRAISEFLAGS(A) arch_raise_fpflag(A)
#define fDF_MAX(A, B) \
    (((A) == (B)) ? fDOUBLE(fUNDOUBLE(A) & fUNDOUBLE(B)) : fmax(A, B))
#define fDF_MIN(A, B) \
    (((A) == (B)) ? fDOUBLE(fUNDOUBLE(A) | fUNDOUBLE(B)) : fmin(A, B))
#define fSF_MAX(A, B) \
    (((A) == (B)) ? fFLOAT(fUNFLOAT(A) & fUNFLOAT(B)) : fmaxf(A, B))
#define fSF_MIN(A, B) \
    (((A) == (B)) ? fFLOAT(fUNFLOAT(A) | fUNFLOAT(B)) : fminf(A, B))

#ifdef QEMU_GENERATE
#define fLOAD(NUM, SIZE, SIGN, EA, DST) MEM_LOAD##SIZE##SIGN(DST, EA)
#else
#define fLOAD(NUM, SIZE, SIGN, EA, DST) \
    DST = (size##SIZE##SIGN##_t)MEM_LOAD##SIZE##SIGN(EA)
#endif

#define fMEMOP(NUM, SIZE, SIGN, EA, FNTYPE, VALUE)

#define fGET_FRAMEKEY() READ_REG(HEX_REG_FRAMEKEY)
#define fFRAME_SCRAMBLE(VAL) ((VAL) ^ (fCAST8u(fGET_FRAMEKEY()) << 32))
#define fFRAME_UNSCRAMBLE(VAL) fFRAME_SCRAMBLE(VAL)

#ifdef CONFIG_USER_ONLY
#define fFRAMECHECK(ADDR, EA) do { } while (0) /* Not modelled in linux-user */
#else
/* System mode not implemented yet */
#define fFRAMECHECK(ADDR, EA)  g_assert_not_reached();
#endif

#ifdef QEMU_GENERATE
#define fLOAD_LOCKED(NUM, SIZE, SIGN, EA, DST) \
    gen_load_locked##SIZE##SIGN(DST, EA, ctx->mem_idx);
#endif

#define fSTORE(NUM, SIZE, EA, SRC) MEM_STORE##SIZE(EA, SRC, slot)

#ifdef QEMU_GENERATE
#define fSTORE_LOCKED(NUM, SIZE, EA, SRC, PRED) \
    gen_store_conditional##SIZE(env, ctx, PdN, PRED, EA, SRC);
#endif

#define fGETBYTE(N, SRC) ((int8_t)((SRC >> ((N) * 8)) & 0xff))
#define fGETUBYTE(N, SRC) ((uint8_t)((SRC >> ((N) * 8)) & 0xff))

#define fSETBYTE(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffLL << ((N) * 8))) | \
        (((uint64_t)((VAL) & 0x0ffLL)) << ((N) * 8)); \
    } while (0)
#define fGETHALF(N, SRC) ((int16_t)((SRC >> ((N) * 16)) & 0xffff))
#define fGETUHALF(N, SRC) ((uint16_t)((SRC >> ((N) * 16)) & 0xffff))
#define fSETHALF(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffLL << ((N) * 16))) | \
        (((uint64_t)((VAL) & 0x0ffff)) << ((N) * 16)); \
    } while (0)
#define fSETHALFw fSETHALF
#define fSETHALFd fSETHALF

#define fGETWORD(N, SRC) \
    ((int64_t)((int32_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))
#define fGETUWORD(N, SRC) \
    ((uint64_t)((uint32_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))

#define fSETWORD(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffffffLL << ((N) * 32))) | \
              (((VAL) & 0x0ffffffffLL) << ((N) * 32)); \
    } while (0)

#define fSETBIT(N, DST, VAL) \
    do { \
        DST = (DST & ~(1ULL << (N))) | (((uint64_t)(VAL)) << (N)); \
    } while (0)

#define fGETBIT(N, SRC) (((SRC) >> N) & 1)
#define fSETBITS(HI, LO, DST, VAL) \
    do { \
        int j; \
        for (j = LO; j <= HI; j++) { \
            fSETBIT(j, DST, VAL); \
        } \
    } while (0)
#define fCOUNTONES_4(VAL) ctpop32(VAL)
#define fCOUNTONES_8(VAL) ctpop64(VAL)
#define fBREV_8(VAL) revbit64(VAL)
#define fBREV_4(VAL) revbit32(VAL)
#define fCL1_8(VAL) clo64(VAL)
#define fCL1_4(VAL) clo32(VAL)
#define fINTERLEAVE(ODD, EVEN) interleave(ODD, EVEN)
#define fDEINTERLEAVE(MIXED) deinterleave(MIXED)
#define fHIDE(A) A
#define fCONSTLL(A) A##LL
#define fECHO(A) (A)

#define fTRAP(TRAPTYPE, IMM) helper_raise_exception(env, HEX_EXCP_TRAP0)

#define fALIGN_REG_FIELD_VALUE(FIELD, VAL) \
    ((VAL) << reg_field_info[FIELD].offset)
#define fGET_REG_FIELD_MASK(FIELD) \
    (((1 << reg_field_info[FIELD].width) - 1) << reg_field_info[FIELD].offset)
#define fREAD_REG_FIELD(REG, FIELD) \
    fEXTRACTU_BITS(env->gpr[HEX_REG_##REG], \
                   reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)
#define fGET_FIELD(VAL, FIELD)
#define fSET_FIELD(VAL, FIELD, NEWVAL)
#define fBARRIER()
#define fSYNCH()
#define fISYNC()
#define fDCFETCH(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fICINVA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fL2FETCH(ADDR, HEIGHT, WIDTH, STRIDE, FLAGS)
#define fDCCLEANA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fDCCLEANINVA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */

#define fDCZEROA(REG) do { env->dczero_addr = (REG); } while (0)

#define fBRANCH_SPECULATE_STALL(DOTNEWVAL, JUMP_COND, SPEC_DIR, HINTBITNUM, \
                                STRBITNUM) /* Nothing */


#endif
