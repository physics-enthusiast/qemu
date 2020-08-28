/*
 *  Xilinx MicroBlaze emulation for qemu: main translation routines.
 *
 *  Copyright (c) 2009 Edgar E. Iglesias.
 *  Copyright (c) 2009-2012 PetaLogix Qld Pty Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/helper-proto.h"
#include "microblaze-decode.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-gen.h"
#include "exec/translator.h"
#include "qemu/qemu-print.h"

#include "trace-tcg.h"
#include "exec/log.h"

#define EXTRACT_FIELD(src, start, end) \
            (((src) >> start) & ((1 << (end - start + 1)) - 1))

/* is_jmp field values */
#define DISAS_JUMP    DISAS_TARGET_0 /* only pc was modified dynamically */
#define DISAS_UPDATE  DISAS_TARGET_1 /* cpu state was modified dynamically */

static TCGv_i32 cpu_R[32];
static TCGv_i32 cpu_pc;
static TCGv_i32 cpu_msr;
static TCGv_i32 cpu_msr_c;
static TCGv_i32 cpu_imm;
static TCGv_i32 cpu_btaken;
static TCGv_i32 cpu_btarget;
static TCGv_i32 cpu_iflags;
static TCGv cpu_res_addr;
static TCGv_i32 cpu_res_val;

#include "exec/gen-icount.h"

/* This is the state at translation time.  */
typedef struct DisasContext {
    DisasContextBase base;
    MicroBlazeCPU *cpu;

    /* Decoder.  */
    int type_b;
    uint32_t ir;
    uint32_t ext_imm;
    uint8_t opcode;
    uint8_t rd, ra, rb;
    uint16_t imm;

    unsigned int cpustate_changed;
    unsigned int delayed_branch;
    unsigned int tb_flags, synced_flags; /* tb dependent flags.  */
    unsigned int clear_imm;

#define JMP_NOJMP     0
#define JMP_DIRECT    1
#define JMP_DIRECT_CC 2
#define JMP_INDIRECT  3
    unsigned int jmp;
    uint32_t jmp_pc;

    int abort_at_next_insn;
} DisasContext;

/* Include the auto-generated decoder.  */
#include "decode-insns.c.inc"

static inline void t_sync_flags(DisasContext *dc)
{
    /* Synch the tb dependent flags between translator and runtime.  */
    if (dc->tb_flags != dc->synced_flags) {
        tcg_gen_movi_i32(cpu_iflags, dc->tb_flags);
        dc->synced_flags = dc->tb_flags;
    }
}

static void gen_raise_exception(DisasContext *dc, uint32_t index)
{
    TCGv_i32 tmp = tcg_const_i32(index);

    gen_helper_raise_exception(cpu_env, tmp);
    tcg_temp_free_i32(tmp);
    dc->base.is_jmp = DISAS_NORETURN;
}

static void gen_raise_exception_sync(DisasContext *dc, uint32_t index)
{
    t_sync_flags(dc);
    tcg_gen_movi_i32(cpu_pc, dc->base.pc_next);
    gen_raise_exception(dc, index);
}

static void gen_raise_hw_excp(DisasContext *dc, uint32_t esr_ec)
{
    TCGv_i32 tmp = tcg_const_i32(esr_ec);
    tcg_gen_st_i32(tmp, cpu_env, offsetof(CPUMBState, esr));
    tcg_temp_free_i32(tmp);

    gen_raise_exception_sync(dc, EXCP_HW_EXCP);
}

static inline bool use_goto_tb(DisasContext *dc, target_ulong dest)
{
#ifndef CONFIG_USER_ONLY
    return (dc->base.pc_first & TARGET_PAGE_MASK) == (dest & TARGET_PAGE_MASK);
#else
    return true;
#endif
}

static void gen_goto_tb(DisasContext *dc, int n, target_ulong dest)
{
    if (dc->base.singlestep_enabled) {
        TCGv_i32 tmp = tcg_const_i32(EXCP_DEBUG);
        tcg_gen_movi_i32(cpu_pc, dest);
        gen_helper_raise_exception(cpu_env, tmp);
        tcg_temp_free_i32(tmp);
    } else if (use_goto_tb(dc, dest)) {
        tcg_gen_goto_tb(n);
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(dc->base.tb, n);
    } else {
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(NULL, 0);
    }
    dc->base.is_jmp = DISAS_NORETURN;
}

/*
 * Returns true if the insn an illegal operation.
 * If exceptions are enabled, an exception is raised.
 */
static bool trap_illegal(DisasContext *dc, bool cond)
{
    if (cond && (dc->tb_flags & MSR_EE_FLAG)
        && dc->cpu->cfg.illegal_opcode_exception) {
        gen_raise_hw_excp(dc, ESR_EC_ILLEGAL_OP);
    }
    return cond;
}

/*
 * Returns true if the insn is illegal in userspace.
 * If exceptions are enabled, an exception is raised.
 */
static bool trap_userspace(DisasContext *dc, bool cond)
{
    int mem_index = cpu_mmu_index(&dc->cpu->env, false);
    bool cond_user = cond && mem_index == MMU_USER_IDX;

    if (cond_user && (dc->tb_flags & MSR_EE_FLAG)) {
        gen_raise_hw_excp(dc, ESR_EC_PRIVINSN);
    }
    return cond_user;
}

static int32_t dec_alu_typeb_imm(DisasContext *dc)
{
    tcg_debug_assert(dc->type_b);
    if (dc->tb_flags & IMM_FLAG) {
        return dc->ext_imm | dc->imm;
    } else {
        return (int16_t)dc->imm;
    }
}

static inline TCGv_i32 *dec_alu_op_b(DisasContext *dc)
{
    if (dc->type_b) {
        tcg_gen_movi_i32(cpu_imm, dec_alu_typeb_imm(dc));
        return &cpu_imm;
    }
    return &cpu_R[dc->rb];
}

static void dec_add(DisasContext *dc)
{
    unsigned int k, c;
    TCGv_i32 cf;

    k = dc->opcode & 4;
    c = dc->opcode & 2;

    /* Take care of the easy cases first.  */
    if (k) {
        /* k - keep carry, no need to update MSR.  */
        /* If rd == r0, it's a nop.  */
        if (dc->rd) {
            tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));

            if (c) {
                /* c - Add carry into the result.  */
                tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->rd], cpu_msr_c);
            }
        }
        return;
    }

    /* From now on, we can assume k is zero.  So we need to update MSR.  */
    /* Extract carry.  */
    cf = tcg_temp_new_i32();
    if (c) {
        tcg_gen_mov_i32(cf, cpu_msr_c);
    } else {
        tcg_gen_movi_i32(cf, 0);
    }

    gen_helper_carry(cpu_msr_c, cpu_R[dc->ra], *(dec_alu_op_b(dc)), cf);
    if (dc->rd) {
        tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
        tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->rd], cf);
    }
    tcg_temp_free_i32(cf);
}

static void dec_sub(DisasContext *dc)
{
    unsigned int u, cmp, k, c;
    TCGv_i32 cf, na;

    u = dc->imm & 2;
    k = dc->opcode & 4;
    c = dc->opcode & 2;
    cmp = (dc->imm & 1) && (!dc->type_b) && k;

    if (cmp) {
        if (dc->rd) {
            if (u)
                gen_helper_cmpu(cpu_R[dc->rd], cpu_R[dc->ra], cpu_R[dc->rb]);
            else
                gen_helper_cmp(cpu_R[dc->rd], cpu_R[dc->ra], cpu_R[dc->rb]);
        }
        return;
    }

    /* Take care of the easy cases first.  */
    if (k) {
        /* k - keep carry, no need to update MSR.  */
        /* If rd == r0, it's a nop.  */
        if (dc->rd) {
            tcg_gen_sub_i32(cpu_R[dc->rd], *(dec_alu_op_b(dc)), cpu_R[dc->ra]);

            if (c) {
                /* c - Add carry into the result.  */
                tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->rd], cpu_msr_c);
            }
        }
        return;
    }

    /* From now on, we can assume k is zero.  So we need to update MSR.  */
    /* Extract carry. And complement a into na.  */
    cf = tcg_temp_new_i32();
    na = tcg_temp_new_i32();
    if (c) {
        tcg_gen_mov_i32(cf, cpu_msr_c);
    } else {
        tcg_gen_movi_i32(cf, 1);
    }

    /* d = b + ~a + c. carry defaults to 1.  */
    tcg_gen_not_i32(na, cpu_R[dc->ra]);

    gen_helper_carry(cpu_msr_c, na, *(dec_alu_op_b(dc)), cf);
    if (dc->rd) {
        tcg_gen_add_i32(cpu_R[dc->rd], na, *(dec_alu_op_b(dc)));
        tcg_gen_add_i32(cpu_R[dc->rd], cpu_R[dc->rd], cf);
    }
    tcg_temp_free_i32(cf);
    tcg_temp_free_i32(na);
}

static void dec_pattern(DisasContext *dc)
{
    unsigned int mode;

    if (trap_illegal(dc, !dc->cpu->cfg.use_pcmp_instr)) {
        return;
    }

    mode = dc->opcode & 3;
    switch (mode) {
        case 0:
            /* pcmpbf.  */
            if (dc->rd)
                gen_helper_pcmpbf(cpu_R[dc->rd], cpu_R[dc->ra], cpu_R[dc->rb]);
            break;
        case 2:
            if (dc->rd) {
                tcg_gen_setcond_i32(TCG_COND_EQ, cpu_R[dc->rd],
                                   cpu_R[dc->ra], cpu_R[dc->rb]);
            }
            break;
        case 3:
            if (dc->rd) {
                tcg_gen_setcond_i32(TCG_COND_NE, cpu_R[dc->rd],
                                   cpu_R[dc->ra], cpu_R[dc->rb]);
            }
            break;
        default:
            cpu_abort(CPU(dc->cpu),
                      "unsupported pattern insn opcode=%x\n", dc->opcode);
            break;
    }
}

static void dec_and(DisasContext *dc)
{
    unsigned int not;

    if (!dc->type_b && (dc->imm & (1 << 10))) {
        dec_pattern(dc);
        return;
    }

    not = dc->opcode & (1 << 1);

    if (!dc->rd)
        return;

    if (not) {
        tcg_gen_andc_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
    } else
        tcg_gen_and_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
}

static void dec_or(DisasContext *dc)
{
    if (!dc->type_b && (dc->imm & (1 << 10))) {
        dec_pattern(dc);
        return;
    }

    if (dc->rd)
        tcg_gen_or_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
}

static void dec_xor(DisasContext *dc)
{
    if (!dc->type_b && (dc->imm & (1 << 10))) {
        dec_pattern(dc);
        return;
    }

    if (dc->rd)
        tcg_gen_xor_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
}

static void msr_read(DisasContext *dc, TCGv_i32 d)
{
    TCGv_i32 t;

    /* Replicate the cpu_msr_c boolean into the proper bit and the copy. */
    t = tcg_temp_new_i32();
    tcg_gen_muli_i32(t, cpu_msr_c, MSR_C | MSR_CC);
    tcg_gen_or_i32(d, cpu_msr, t);
    tcg_temp_free_i32(t);
}

static void msr_write(DisasContext *dc, TCGv_i32 v)
{
    dc->cpustate_changed = 1;

    /* Install MSR_C.  */
    tcg_gen_extract_i32(cpu_msr_c, v, 2, 1);

    /* Clear MSR_C and MSR_CC; MSR_PVR is not writable, and is always clear. */
    tcg_gen_andi_i32(cpu_msr, v, ~(MSR_C | MSR_CC | MSR_PVR));
}

static void dec_msr(DisasContext *dc)
{
    CPUState *cs = CPU(dc->cpu);
    TCGv_i32 t0, t1;
    unsigned int sr, rn;
    bool to, clrset, extended = false;

    sr = extract32(dc->imm, 0, 14);
    to = extract32(dc->imm, 14, 1);
    clrset = extract32(dc->imm, 15, 1) == 0;
    dc->type_b = 1;
    if (to) {
        dc->cpustate_changed = 1;
    }

    /* Extended MSRs are only available if addr_size > 32.  */
    if (dc->cpu->cfg.addr_size > 32) {
        /* The E-bit is encoded differently for To/From MSR.  */
        static const unsigned int e_bit[] = { 19, 24 };

        extended = extract32(dc->imm, e_bit[to], 1);
    }

    /* msrclr and msrset.  */
    if (clrset) {
        bool clr = extract32(dc->ir, 16, 1);

        if (!dc->cpu->cfg.use_msr_instr) {
            /* nop??? */
            return;
        }

        if (trap_userspace(dc, dc->imm != 4 && dc->imm != 0)) {
            return;
        }

        if (dc->rd)
            msr_read(dc, cpu_R[dc->rd]);

        t0 = tcg_temp_new_i32();
        t1 = tcg_temp_new_i32();
        msr_read(dc, t0);
        tcg_gen_mov_i32(t1, *(dec_alu_op_b(dc)));

        if (clr) {
            tcg_gen_not_i32(t1, t1);
            tcg_gen_and_i32(t0, t0, t1);
        } else
            tcg_gen_or_i32(t0, t0, t1);
        msr_write(dc, t0);
        tcg_temp_free_i32(t0);
        tcg_temp_free_i32(t1);
        tcg_gen_movi_i32(cpu_pc, dc->base.pc_next + 4);
        dc->base.is_jmp = DISAS_UPDATE;
        return;
    }

    if (trap_userspace(dc, to)) {
        return;
    }

#if !defined(CONFIG_USER_ONLY)
    /* Catch read/writes to the mmu block.  */
    if ((sr & ~0xff) == 0x1000) {
        TCGv_i32 tmp_ext = tcg_const_i32(extended);
        TCGv_i32 tmp_sr;

        sr &= 7;
        tmp_sr = tcg_const_i32(sr);
        if (to) {
            gen_helper_mmu_write(cpu_env, tmp_ext, tmp_sr, cpu_R[dc->ra]);
        } else {
            gen_helper_mmu_read(cpu_R[dc->rd], cpu_env, tmp_ext, tmp_sr);
        }
        tcg_temp_free_i32(tmp_sr);
        tcg_temp_free_i32(tmp_ext);
        return;
    }
#endif

    if (to) {
        switch (sr) {
            case SR_PC:
                break;
            case SR_MSR:
                msr_write(dc, cpu_R[dc->ra]);
                break;
            case SR_EAR:
                {
                    TCGv_i64 t64 = tcg_temp_new_i64();
                    tcg_gen_extu_i32_i64(t64, cpu_R[dc->ra]);
                    tcg_gen_st_i64(t64, cpu_env, offsetof(CPUMBState, ear));
                    tcg_temp_free_i64(t64);
                }
                break;
            case SR_ESR:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, esr));
                break;
            case SR_FSR:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, fsr));
                break;
            case SR_BTR:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, btr));
                break;
            case SR_EDR:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, edr));
                break;
            case 0x800:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, slr));
                break;
            case 0x802:
                tcg_gen_st_i32(cpu_R[dc->ra],
                               cpu_env, offsetof(CPUMBState, shr));
                break;
            default:
                cpu_abort(CPU(dc->cpu), "unknown mts reg %x\n", sr);
                break;
        }
    } else {
        switch (sr) {
            case SR_PC:
                tcg_gen_movi_i32(cpu_R[dc->rd], dc->base.pc_next);
                break;
            case SR_MSR:
                msr_read(dc, cpu_R[dc->rd]);
                break;
            case SR_EAR:
                {
                    TCGv_i64 t64 = tcg_temp_new_i64();
                    tcg_gen_ld_i64(t64, cpu_env, offsetof(CPUMBState, ear));
                    if (extended) {
                        tcg_gen_extrh_i64_i32(cpu_R[dc->rd], t64);
                    } else {
                        tcg_gen_extrl_i64_i32(cpu_R[dc->rd], t64);
                    }
                    tcg_temp_free_i64(t64);
                }
                break;
            case SR_ESR:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, esr));
                break;
            case SR_FSR:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, fsr));
                break;
            case SR_BTR:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, btr));
                break;
            case SR_EDR:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, edr));
                break;
            case 0x800:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, slr));
                break;
            case 0x802:
                tcg_gen_ld_i32(cpu_R[dc->rd],
                               cpu_env, offsetof(CPUMBState, shr));
                break;
            case 0x2000 ... 0x200c:
                rn = sr & 0xf;
                tcg_gen_ld_i32(cpu_R[dc->rd],
                              cpu_env, offsetof(CPUMBState, pvr.regs[rn]));
                break;
            default:
                cpu_abort(cs, "unknown mfs reg %x\n", sr);
                break;
        }
    }

    if (dc->rd == 0) {
        tcg_gen_movi_i32(cpu_R[0], 0);
    }
}

/* Multiplier unit.  */
static void dec_mul(DisasContext *dc)
{
    TCGv_i32 tmp;
    unsigned int subcode;

    if (trap_illegal(dc, !dc->cpu->cfg.use_hw_mul)) {
        return;
    }

    subcode = dc->imm & 3;

    if (dc->type_b) {
        tcg_gen_mul_i32(cpu_R[dc->rd], cpu_R[dc->ra], *(dec_alu_op_b(dc)));
        return;
    }

    /* mulh, mulhsu and mulhu are not available if C_USE_HW_MUL is < 2.  */
    if (subcode >= 1 && subcode <= 3 && dc->cpu->cfg.use_hw_mul < 2) {
        /* nop??? */
    }

    tmp = tcg_temp_new_i32();
    switch (subcode) {
        case 0:
            tcg_gen_mul_i32(cpu_R[dc->rd], cpu_R[dc->ra], cpu_R[dc->rb]);
            break;
        case 1:
            tcg_gen_muls2_i32(tmp, cpu_R[dc->rd],
                              cpu_R[dc->ra], cpu_R[dc->rb]);
            break;
        case 2:
            tcg_gen_mulsu2_i32(tmp, cpu_R[dc->rd],
                               cpu_R[dc->ra], cpu_R[dc->rb]);
            break;
        case 3:
            tcg_gen_mulu2_i32(tmp, cpu_R[dc->rd], cpu_R[dc->ra], cpu_R[dc->rb]);
            break;
        default:
            cpu_abort(CPU(dc->cpu), "unknown MUL insn %x\n", subcode);
            break;
    }
    tcg_temp_free_i32(tmp);
}

/* Div unit.  */
static void dec_div(DisasContext *dc)
{
    unsigned int u;

    u = dc->imm & 2; 

    if (trap_illegal(dc, !dc->cpu->cfg.use_div)) {
        return;
    }

    if (u)
        gen_helper_divu(cpu_R[dc->rd], cpu_env, *(dec_alu_op_b(dc)),
                        cpu_R[dc->ra]);
    else
        gen_helper_divs(cpu_R[dc->rd], cpu_env, *(dec_alu_op_b(dc)),
                        cpu_R[dc->ra]);
    if (!dc->rd)
        tcg_gen_movi_i32(cpu_R[dc->rd], 0);
}

static void dec_barrel(DisasContext *dc)
{
    TCGv_i32 t0;
    unsigned int imm_w, imm_s;
    bool s, t, e = false, i = false;

    if (trap_illegal(dc, !dc->cpu->cfg.use_barrel)) {
        return;
    }

    if (dc->type_b) {
        /* Insert and extract are only available in immediate mode.  */
        i = extract32(dc->imm, 15, 1);
        e = extract32(dc->imm, 14, 1);
    }
    s = extract32(dc->imm, 10, 1);
    t = extract32(dc->imm, 9, 1);
    imm_w = extract32(dc->imm, 6, 5);
    imm_s = extract32(dc->imm, 0, 5);

    if (e) {
        if (imm_w + imm_s > 32 || imm_w == 0) {
            /* These inputs have an undefined behavior.  */
            qemu_log_mask(LOG_GUEST_ERROR, "bsefi: Bad input w=%d s=%d\n",
                          imm_w, imm_s);
        } else {
            tcg_gen_extract_i32(cpu_R[dc->rd], cpu_R[dc->ra], imm_s, imm_w);
        }
    } else if (i) {
        int width = imm_w - imm_s + 1;

        if (imm_w < imm_s) {
            /* These inputs have an undefined behavior.  */
            qemu_log_mask(LOG_GUEST_ERROR, "bsifi: Bad input w=%d s=%d\n",
                          imm_w, imm_s);
        } else {
            tcg_gen_deposit_i32(cpu_R[dc->rd], cpu_R[dc->rd], cpu_R[dc->ra],
                                imm_s, width);
        }
    } else {
        t0 = tcg_temp_new_i32();

        tcg_gen_mov_i32(t0, *(dec_alu_op_b(dc)));
        tcg_gen_andi_i32(t0, t0, 31);

        if (s) {
            tcg_gen_shl_i32(cpu_R[dc->rd], cpu_R[dc->ra], t0);
        } else {
            if (t) {
                tcg_gen_sar_i32(cpu_R[dc->rd], cpu_R[dc->ra], t0);
            } else {
                tcg_gen_shr_i32(cpu_R[dc->rd], cpu_R[dc->ra], t0);
            }
        }
        tcg_temp_free_i32(t0);
    }
}

static void dec_bit(DisasContext *dc)
{
    CPUState *cs = CPU(dc->cpu);
    TCGv_i32 t0;
    unsigned int op;

    op = dc->ir & ((1 << 9) - 1);
    switch (op) {
        case 0x21:
            /* src.  */
            t0 = tcg_temp_new_i32();

            tcg_gen_shli_i32(t0, cpu_msr_c, 31);
            tcg_gen_andi_i32(cpu_msr_c, cpu_R[dc->ra], 1);
            if (dc->rd) {
                tcg_gen_shri_i32(cpu_R[dc->rd], cpu_R[dc->ra], 1);
                tcg_gen_or_i32(cpu_R[dc->rd], cpu_R[dc->rd], t0);
            }
            tcg_temp_free_i32(t0);
            break;

        case 0x1:
        case 0x41:
            /* srl.  */
            tcg_gen_andi_i32(cpu_msr_c, cpu_R[dc->ra], 1);
            if (dc->rd) {
                if (op == 0x41)
                    tcg_gen_shri_i32(cpu_R[dc->rd], cpu_R[dc->ra], 1);
                else
                    tcg_gen_sari_i32(cpu_R[dc->rd], cpu_R[dc->ra], 1);
            }
            break;
        case 0x60:
            tcg_gen_ext8s_i32(cpu_R[dc->rd], cpu_R[dc->ra]);
            break;
        case 0x61:
            tcg_gen_ext16s_i32(cpu_R[dc->rd], cpu_R[dc->ra]);
            break;
        case 0x64:
        case 0x66:
        case 0x74:
        case 0x76:
            /* wdc.  */
            trap_userspace(dc, true);
            break;
        case 0x68:
            /* wic.  */
            trap_userspace(dc, true);
            break;
        case 0xe0:
            if (trap_illegal(dc, !dc->cpu->cfg.use_pcmp_instr)) {
                return;
            }
            if (dc->cpu->cfg.use_pcmp_instr) {
                tcg_gen_clzi_i32(cpu_R[dc->rd], cpu_R[dc->ra], 32);
            }
            break;
        case 0x1e0:
            /* swapb */
            tcg_gen_bswap32_i32(cpu_R[dc->rd], cpu_R[dc->ra]);
            break;
        case 0x1e2:
            /*swaph */
            tcg_gen_rotri_i32(cpu_R[dc->rd], cpu_R[dc->ra], 16);
            break;
        default:
            cpu_abort(cs, "unknown bit oc=%x op=%x rd=%d ra=%d rb=%d\n",
                      (uint32_t)dc->base.pc_next, op, dc->rd, dc->ra, dc->rb);
            break;
    }
}

static inline void sync_jmpstate(DisasContext *dc)
{
    if (dc->jmp == JMP_DIRECT || dc->jmp == JMP_DIRECT_CC) {
        if (dc->jmp == JMP_DIRECT) {
            tcg_gen_movi_i32(cpu_btaken, 1);
        }
        dc->jmp = JMP_INDIRECT;
        tcg_gen_movi_i32(cpu_btarget, dc->jmp_pc);
    }
}

static void dec_imm(DisasContext *dc)
{
    dc->ext_imm = dc->imm << 16;
    tcg_gen_movi_i32(cpu_imm, dc->ext_imm);
    dc->tb_flags |= IMM_FLAG;
    dc->clear_imm = 0;
}

static inline void compute_ldst_addr(DisasContext *dc, bool ea, TCGv t)
{
    /* Should be set to true if r1 is used by loadstores.  */
    bool stackprot = false;
    TCGv_i32 t32;

    /* All load/stores use ra.  */
    if (dc->ra == 1 && dc->cpu->cfg.stackprot) {
        stackprot = true;
    }

    /* Treat the common cases first.  */
    if (!dc->type_b) {
        if (ea) {
            int addr_size = dc->cpu->cfg.addr_size;

            if (addr_size == 32) {
                tcg_gen_extu_i32_tl(t, cpu_R[dc->rb]);
                return;
            }

            tcg_gen_concat_i32_i64(t, cpu_R[dc->rb], cpu_R[dc->ra]);
            if (addr_size < 64) {
                /* Mask off out of range bits.  */
                tcg_gen_andi_i64(t, t, MAKE_64BIT_MASK(0, addr_size));
            }
            return;
        }

        /* If any of the regs is r0, set t to the value of the other reg.  */
        if (dc->ra == 0) {
            tcg_gen_extu_i32_tl(t, cpu_R[dc->rb]);
            return;
        } else if (dc->rb == 0) {
            tcg_gen_extu_i32_tl(t, cpu_R[dc->ra]);
            return;
        }

        if (dc->rb == 1 && dc->cpu->cfg.stackprot) {
            stackprot = true;
        }

        t32 = tcg_temp_new_i32();
        tcg_gen_add_i32(t32, cpu_R[dc->ra], cpu_R[dc->rb]);
        tcg_gen_extu_i32_tl(t, t32);
        tcg_temp_free_i32(t32);

        if (stackprot) {
            gen_helper_stackprot(cpu_env, t);
        }
        return;
    }
    /* Immediate.  */
    t32 = tcg_temp_new_i32();
    tcg_gen_addi_i32(t32, cpu_R[dc->ra], dec_alu_typeb_imm(dc));
    tcg_gen_extu_i32_tl(t, t32);
    tcg_temp_free_i32(t32);

    if (stackprot) {
        gen_helper_stackprot(cpu_env, t);
    }
    return;
}

static void dec_load(DisasContext *dc)
{
    TCGv_i32 v;
    TCGv addr;
    unsigned int size;
    bool rev = false, ex = false, ea = false;
    int mem_index = cpu_mmu_index(&dc->cpu->env, false);
    MemOp mop;

    mop = dc->opcode & 3;
    size = 1 << mop;
    if (!dc->type_b) {
        ea = extract32(dc->ir, 7, 1);
        rev = extract32(dc->ir, 9, 1);
        ex = extract32(dc->ir, 10, 1);
    }
    mop |= MO_TE;
    if (rev) {
        mop ^= MO_BSWAP;
    }

    if (trap_illegal(dc, size > 4)) {
        return;
    }

    if (trap_userspace(dc, ea)) {
        return;
    }

    t_sync_flags(dc);
    addr = tcg_temp_new();
    compute_ldst_addr(dc, ea, addr);
    /* Extended addressing bypasses the MMU.  */
    mem_index = ea ? MMU_NOMMU_IDX : mem_index;

    /*
     * When doing reverse accesses we need to do two things.
     *
     * 1. Reverse the address wrt endianness.
     * 2. Byteswap the data lanes on the way back into the CPU core.
     */
    if (rev && size != 4) {
        /* Endian reverse the address. t is addr.  */
        switch (size) {
            case 1:
            {
                tcg_gen_xori_tl(addr, addr, 3);
                break;
            }

            case 2:
                /* 00 -> 10
                   10 -> 00.  */
                tcg_gen_xori_tl(addr, addr, 2);
                break;
            default:
                cpu_abort(CPU(dc->cpu), "Invalid reverse size\n");
                break;
        }
    }

    /* lwx does not throw unaligned access errors, so force alignment */
    if (ex) {
        tcg_gen_andi_tl(addr, addr, ~3);
    }

    /* If we get a fault on a dslot, the jmpstate better be in sync.  */
    sync_jmpstate(dc);

    /* Verify alignment if needed.  */
    /*
     * Microblaze gives MMU faults priority over faults due to
     * unaligned addresses. That's why we speculatively do the load
     * into v. If the load succeeds, we verify alignment of the
     * address and if that succeeds we write into the destination reg.
     */
    v = tcg_temp_new_i32();
    tcg_gen_qemu_ld_i32(v, addr, mem_index, mop);

    if (dc->cpu->cfg.unaligned_exceptions && size > 1) {
        TCGv_i32 t0 = tcg_const_i32(0);
        TCGv_i32 treg = tcg_const_i32(dc->rd);
        TCGv_i32 tsize = tcg_const_i32(size - 1);

        tcg_gen_movi_i32(cpu_pc, dc->base.pc_next);
        gen_helper_memalign(cpu_env, addr, treg, t0, tsize);

        tcg_temp_free_i32(t0);
        tcg_temp_free_i32(treg);
        tcg_temp_free_i32(tsize);
    }

    if (ex) {
        tcg_gen_mov_tl(cpu_res_addr, addr);
        tcg_gen_mov_i32(cpu_res_val, v);
    }
    if (dc->rd) {
        tcg_gen_mov_i32(cpu_R[dc->rd], v);
    }
    tcg_temp_free_i32(v);

    if (ex) { /* lwx */
        /* no support for AXI exclusive so always clear C */
        tcg_gen_movi_i32(cpu_msr_c, 0);
    }

    tcg_temp_free(addr);
}

static void dec_store(DisasContext *dc)
{
    TCGv addr;
    TCGLabel *swx_skip = NULL;
    unsigned int size;
    bool rev = false, ex = false, ea = false;
    int mem_index = cpu_mmu_index(&dc->cpu->env, false);
    MemOp mop;

    mop = dc->opcode & 3;
    size = 1 << mop;
    if (!dc->type_b) {
        ea = extract32(dc->ir, 7, 1);
        rev = extract32(dc->ir, 9, 1);
        ex = extract32(dc->ir, 10, 1);
    }
    mop |= MO_TE;
    if (rev) {
        mop ^= MO_BSWAP;
    }

    if (trap_illegal(dc, size > 4)) {
        return;
    }

    trap_userspace(dc, ea);

    t_sync_flags(dc);
    /* If we get a fault on a dslot, the jmpstate better be in sync.  */
    sync_jmpstate(dc);
    /* SWX needs a temp_local.  */
    addr = ex ? tcg_temp_local_new() : tcg_temp_new();
    compute_ldst_addr(dc, ea, addr);
    /* Extended addressing bypasses the MMU.  */
    mem_index = ea ? MMU_NOMMU_IDX : mem_index;

    if (ex) { /* swx */
        TCGv_i32 tval;

        /* swx does not throw unaligned access errors, so force alignment */
        tcg_gen_andi_tl(addr, addr, ~3);

        tcg_gen_movi_i32(cpu_msr_c, 1);
        swx_skip = gen_new_label();
        tcg_gen_brcond_tl(TCG_COND_NE, cpu_res_addr, addr, swx_skip);

        /*
         * Compare the value loaded at lwx with current contents of
         * the reserved location.
         */
        tval = tcg_temp_new_i32();

        tcg_gen_atomic_cmpxchg_i32(tval, addr, cpu_res_val,
                                   cpu_R[dc->rd], mem_index,
                                   mop);

        tcg_gen_brcond_i32(TCG_COND_NE, cpu_res_val, tval, swx_skip);
        tcg_gen_movi_i32(cpu_msr_c, 0);
        tcg_temp_free_i32(tval);
    }

    if (rev && size != 4) {
        /* Endian reverse the address. t is addr.  */
        switch (size) {
            case 1:
            {
                tcg_gen_xori_tl(addr, addr, 3);
                break;
            }

            case 2:
                /* 00 -> 10
                   10 -> 00.  */
                /* Force addr into the temp.  */
                tcg_gen_xori_tl(addr, addr, 2);
                break;
            default:
                cpu_abort(CPU(dc->cpu), "Invalid reverse size\n");
                break;
        }
    }

    if (!ex) {
        tcg_gen_qemu_st_i32(cpu_R[dc->rd], addr, mem_index, mop);
    }

    /* Verify alignment if needed.  */
    if (dc->cpu->cfg.unaligned_exceptions && size > 1) {
        TCGv_i32 t1 = tcg_const_i32(1);
        TCGv_i32 treg = tcg_const_i32(dc->rd);
        TCGv_i32 tsize = tcg_const_i32(size - 1);

        tcg_gen_movi_i32(cpu_pc, dc->base.pc_next);
        /* FIXME: if the alignment is wrong, we should restore the value
         *        in memory. One possible way to achieve this is to probe
         *        the MMU prior to the memaccess, thay way we could put
         *        the alignment checks in between the probe and the mem
         *        access.
         */
        gen_helper_memalign(cpu_env, addr, treg, t1, tsize);

        tcg_temp_free_i32(t1);
        tcg_temp_free_i32(treg);
        tcg_temp_free_i32(tsize);
    }

    if (ex) {
        gen_set_label(swx_skip);
    }

    tcg_temp_free(addr);
}

static inline void eval_cc(DisasContext *dc, unsigned int cc,
                           TCGv_i32 d, TCGv_i32 a)
{
    static const int mb_to_tcg_cc[] = {
        [CC_EQ] = TCG_COND_EQ,
        [CC_NE] = TCG_COND_NE,
        [CC_LT] = TCG_COND_LT,
        [CC_LE] = TCG_COND_LE,
        [CC_GE] = TCG_COND_GE,
        [CC_GT] = TCG_COND_GT,
    };

    switch (cc) {
    case CC_EQ:
    case CC_NE:
    case CC_LT:
    case CC_LE:
    case CC_GE:
    case CC_GT:
        tcg_gen_setcondi_i32(mb_to_tcg_cc[cc], d, a, 0);
        break;
    default:
        cpu_abort(CPU(dc->cpu), "Unknown condition code %x.\n", cc);
        break;
    }
}

static void eval_cond_jmp(DisasContext *dc, TCGv_i32 pc_true, TCGv_i32 pc_false)
{
    TCGv_i32 zero = tcg_const_i32(0);

    tcg_gen_movcond_i32(TCG_COND_NE, cpu_pc,
                        cpu_btaken, zero,
                        pc_true, pc_false);

    tcg_temp_free_i32(zero);
}

static void dec_setup_dslot(DisasContext *dc)
{
        TCGv_i32 tmp = tcg_const_i32(dc->type_b && (dc->tb_flags & IMM_FLAG));

        dc->delayed_branch = 2;
        dc->tb_flags |= D_FLAG;

        tcg_gen_st_i32(tmp, cpu_env, offsetof(CPUMBState, bimm));
        tcg_temp_free_i32(tmp);
}

static void dec_bcc(DisasContext *dc)
{
    unsigned int cc;
    unsigned int dslot;

    cc = EXTRACT_FIELD(dc->ir, 21, 23);
    dslot = dc->ir & (1 << 25);

    dc->delayed_branch = 1;
    if (dslot) {
        dec_setup_dslot(dc);
    }

    if (dc->type_b) {
        dc->jmp = JMP_DIRECT_CC;
        dc->jmp_pc = dc->base.pc_next + dec_alu_typeb_imm(dc);
        tcg_gen_movi_i32(cpu_btarget, dc->jmp_pc);
    } else {
        dc->jmp = JMP_INDIRECT;
        tcg_gen_addi_i32(cpu_btarget, cpu_R[dc->rb], dc->base.pc_next);
    }
    eval_cc(dc, cc, cpu_btaken, cpu_R[dc->ra]);
}

static void dec_br(DisasContext *dc)
{
    unsigned int dslot, link, abs, mbar;

    dslot = dc->ir & (1 << 20);
    abs = dc->ir & (1 << 19);
    link = dc->ir & (1 << 18);

    /* Memory barrier.  */
    mbar = (dc->ir >> 16) & 31;
    if (mbar == 2 && dc->imm == 4) {
        uint16_t mbar_imm = dc->rd;

        /* Data access memory barrier.  */
        if ((mbar_imm & 2) == 0) {
            tcg_gen_mb(TCG_BAR_SC | TCG_MO_ALL);
        }

        /* mbar IMM & 16 decodes to sleep.  */
        if (mbar_imm & 16) {
            TCGv_i32 tmp_1;

            if (trap_userspace(dc, true)) {
                /* Sleep is a privileged instruction.  */
                return;
            }

            t_sync_flags(dc);

            tmp_1 = tcg_const_i32(1);
            tcg_gen_st_i32(tmp_1, cpu_env,
                           -offsetof(MicroBlazeCPU, env)
                           +offsetof(CPUState, halted));
            tcg_temp_free_i32(tmp_1);

            tcg_gen_movi_i32(cpu_pc, dc->base.pc_next + 4);

            gen_raise_exception(dc, EXCP_HLT);
            return;
        }
        /* Break the TB.  */
        dc->cpustate_changed = 1;
        return;
    }

    if (abs && link && !dslot) {
        if (dc->type_b) {
            /* BRKI */
            uint32_t imm = dec_alu_typeb_imm(dc);
            if (trap_userspace(dc, imm != 8 && imm != 0x18)) {
                return;
            }
        } else {
            /* BRK */
            if (trap_userspace(dc, true)) {
                return;
            }
        }
    }

    dc->delayed_branch = 1;
    if (dslot) {
        dec_setup_dslot(dc);
    }
    if (link && dc->rd) {
        tcg_gen_movi_i32(cpu_R[dc->rd], dc->base.pc_next);
    }

    if (abs) {
        if (dc->type_b) {
            uint32_t dest = dec_alu_typeb_imm(dc);

            dc->jmp = JMP_DIRECT;
            dc->jmp_pc = dest;
            tcg_gen_movi_i32(cpu_btarget, dest);
            if (link && !dslot) {
                switch (dest) {
                case 8:
                case 0x18:
                    gen_raise_exception_sync(dc, EXCP_BREAK);
                    break;
                case 0:
                    gen_raise_exception_sync(dc, EXCP_DEBUG);
                    break;
                }
            }
        } else {
            dc->jmp = JMP_INDIRECT;
            tcg_gen_mov_i32(cpu_btarget, cpu_R[dc->rb]);
            if (link && !dslot) {
                gen_raise_exception_sync(dc, EXCP_BREAK);
            }
        }
    } else if (dc->type_b) {
        dc->jmp = JMP_DIRECT;
        dc->jmp_pc = dc->base.pc_next + dec_alu_typeb_imm(dc);
        tcg_gen_movi_i32(cpu_btarget, dc->jmp_pc);
    } else {
        dc->jmp = JMP_INDIRECT;
        tcg_gen_addi_i32(cpu_btarget, cpu_R[dc->rb], dc->base.pc_next);
    }
    tcg_gen_movi_i32(cpu_btaken, 1);
}

static inline void do_rti(DisasContext *dc)
{
    TCGv_i32 t0, t1;
    t0 = tcg_temp_new_i32();
    t1 = tcg_temp_new_i32();
    tcg_gen_mov_i32(t1, cpu_msr);
    tcg_gen_shri_i32(t0, t1, 1);
    tcg_gen_ori_i32(t1, t1, MSR_IE);
    tcg_gen_andi_i32(t0, t0, (MSR_VM | MSR_UM));

    tcg_gen_andi_i32(t1, t1, ~(MSR_VM | MSR_UM));
    tcg_gen_or_i32(t1, t1, t0);
    msr_write(dc, t1);
    tcg_temp_free_i32(t1);
    tcg_temp_free_i32(t0);
    dc->tb_flags &= ~DRTI_FLAG;
}

static inline void do_rtb(DisasContext *dc)
{
    TCGv_i32 t0, t1;
    t0 = tcg_temp_new_i32();
    t1 = tcg_temp_new_i32();
    tcg_gen_mov_i32(t1, cpu_msr);
    tcg_gen_andi_i32(t1, t1, ~MSR_BIP);
    tcg_gen_shri_i32(t0, t1, 1);
    tcg_gen_andi_i32(t0, t0, (MSR_VM | MSR_UM));

    tcg_gen_andi_i32(t1, t1, ~(MSR_VM | MSR_UM));
    tcg_gen_or_i32(t1, t1, t0);
    msr_write(dc, t1);
    tcg_temp_free_i32(t1);
    tcg_temp_free_i32(t0);
    dc->tb_flags &= ~DRTB_FLAG;
}

static inline void do_rte(DisasContext *dc)
{
    TCGv_i32 t0, t1;
    t0 = tcg_temp_new_i32();
    t1 = tcg_temp_new_i32();

    tcg_gen_mov_i32(t1, cpu_msr);
    tcg_gen_ori_i32(t1, t1, MSR_EE);
    tcg_gen_andi_i32(t1, t1, ~MSR_EIP);
    tcg_gen_shri_i32(t0, t1, 1);
    tcg_gen_andi_i32(t0, t0, (MSR_VM | MSR_UM));

    tcg_gen_andi_i32(t1, t1, ~(MSR_VM | MSR_UM));
    tcg_gen_or_i32(t1, t1, t0);
    msr_write(dc, t1);
    tcg_temp_free_i32(t1);
    tcg_temp_free_i32(t0);
    dc->tb_flags &= ~DRTE_FLAG;
}

static void dec_rts(DisasContext *dc)
{
    unsigned int b_bit, i_bit, e_bit;

    i_bit = dc->ir & (1 << 21);
    b_bit = dc->ir & (1 << 22);
    e_bit = dc->ir & (1 << 23);

    if (trap_userspace(dc, i_bit || b_bit || e_bit)) {
        return;
    }

    dec_setup_dslot(dc);

    if (i_bit) {
        dc->tb_flags |= DRTI_FLAG;
    } else if (b_bit) {
        dc->tb_flags |= DRTB_FLAG;
    } else if (e_bit) {
        dc->tb_flags |= DRTE_FLAG;
    }

    dc->jmp = JMP_INDIRECT;
    tcg_gen_movi_i32(cpu_btaken, 1);
    tcg_gen_add_i32(cpu_btarget, cpu_R[dc->ra], *dec_alu_op_b(dc));
}

static int dec_check_fpuv2(DisasContext *dc)
{
    if ((dc->cpu->cfg.use_fpu != 2) && (dc->tb_flags & MSR_EE_FLAG)) {
        gen_raise_hw_excp(dc, ESR_EC_FPU);
    }
    return (dc->cpu->cfg.use_fpu == 2) ? PVR2_USE_FPU2_MASK : 0;
}

static void dec_fpu(DisasContext *dc)
{
    unsigned int fpu_insn;

    if (trap_illegal(dc, !dc->cpu->cfg.use_fpu)) {
        return;
    }

    fpu_insn = (dc->ir >> 7) & 7;

    switch (fpu_insn) {
        case 0:
            gen_helper_fadd(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra],
                            cpu_R[dc->rb]);
            break;

        case 1:
            gen_helper_frsub(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra],
                             cpu_R[dc->rb]);
            break;

        case 2:
            gen_helper_fmul(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra],
                            cpu_R[dc->rb]);
            break;

        case 3:
            gen_helper_fdiv(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra],
                            cpu_R[dc->rb]);
            break;

        case 4:
            switch ((dc->ir >> 4) & 7) {
                case 0:
                    gen_helper_fcmp_un(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 1:
                    gen_helper_fcmp_lt(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 2:
                    gen_helper_fcmp_eq(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 3:
                    gen_helper_fcmp_le(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 4:
                    gen_helper_fcmp_gt(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 5:
                    gen_helper_fcmp_ne(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                case 6:
                    gen_helper_fcmp_ge(cpu_R[dc->rd], cpu_env,
                                       cpu_R[dc->ra], cpu_R[dc->rb]);
                    break;
                default:
                    qemu_log_mask(LOG_UNIMP,
                                  "unimplemented fcmp fpu_insn=%x pc=%x"
                                  " opc=%x\n",
                                  fpu_insn, (uint32_t)dc->base.pc_next,
                                  dc->opcode);
                    dc->abort_at_next_insn = 1;
                    break;
            }
            break;

        case 5:
            if (!dec_check_fpuv2(dc)) {
                return;
            }
            gen_helper_flt(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra]);
            break;

        case 6:
            if (!dec_check_fpuv2(dc)) {
                return;
            }
            gen_helper_fint(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra]);
            break;

        case 7:
            if (!dec_check_fpuv2(dc)) {
                return;
            }
            gen_helper_fsqrt(cpu_R[dc->rd], cpu_env, cpu_R[dc->ra]);
            break;

        default:
            qemu_log_mask(LOG_UNIMP, "unimplemented FPU insn fpu_insn=%x pc=%x"
                          " opc=%x\n",
                          fpu_insn, (uint32_t)dc->base.pc_next, dc->opcode);
            dc->abort_at_next_insn = 1;
            break;
    }
}

static void dec_null(DisasContext *dc)
{
    if (trap_illegal(dc, true)) {
        return;
    }
    qemu_log_mask(LOG_GUEST_ERROR, "unknown insn pc=%x opc=%x\n",
                  (uint32_t)dc->base.pc_next, dc->opcode);
    dc->abort_at_next_insn = 1;
}

/* Insns connected to FSL or AXI stream attached devices.  */
static void dec_stream(DisasContext *dc)
{
    TCGv_i32 t_id, t_ctrl;
    int ctrl;

    if (trap_userspace(dc, true)) {
        return;
    }

    t_id = tcg_temp_new_i32();
    if (dc->type_b) {
        tcg_gen_movi_i32(t_id, dc->imm & 0xf);
        ctrl = dc->imm >> 10;
    } else {
        tcg_gen_andi_i32(t_id, cpu_R[dc->rb], 0xf);
        ctrl = dc->imm >> 5;
    }

    t_ctrl = tcg_const_i32(ctrl);

    if (dc->rd == 0) {
        gen_helper_put(t_id, t_ctrl, cpu_R[dc->ra]);
    } else {
        gen_helper_get(cpu_R[dc->rd], t_id, t_ctrl);
    }
    tcg_temp_free_i32(t_id);
    tcg_temp_free_i32(t_ctrl);
}

static struct decoder_info {
    struct {
        uint32_t bits;
        uint32_t mask;
    };
    void (*dec)(DisasContext *dc);
} decinfo[] = {
    {DEC_ADD, dec_add},
    {DEC_SUB, dec_sub},
    {DEC_AND, dec_and},
    {DEC_XOR, dec_xor},
    {DEC_OR, dec_or},
    {DEC_BIT, dec_bit},
    {DEC_BARREL, dec_barrel},
    {DEC_LD, dec_load},
    {DEC_ST, dec_store},
    {DEC_IMM, dec_imm},
    {DEC_BR, dec_br},
    {DEC_BCC, dec_bcc},
    {DEC_RTS, dec_rts},
    {DEC_FPU, dec_fpu},
    {DEC_MUL, dec_mul},
    {DEC_DIV, dec_div},
    {DEC_MSR, dec_msr},
    {DEC_STREAM, dec_stream},
    {{0, 0}, dec_null}
};

static void old_decode(DisasContext *dc, uint32_t ir)
{
    int i;

    dc->ir = ir;

    if (ir == 0) {
        trap_illegal(dc, dc->cpu->cfg.opcode_0_illegal);
        /* Don't decode nop/zero instructions any further.  */
        return;
    }

    /* bit 2 seems to indicate insn type.  */
    dc->type_b = ir & (1 << 29);

    dc->opcode = EXTRACT_FIELD(ir, 26, 31);
    dc->rd = EXTRACT_FIELD(ir, 21, 25);
    dc->ra = EXTRACT_FIELD(ir, 16, 20);
    dc->rb = EXTRACT_FIELD(ir, 11, 15);
    dc->imm = EXTRACT_FIELD(ir, 0, 15);

    /* Large switch for all insns.  */
    for (i = 0; i < ARRAY_SIZE(decinfo); i++) {
        if ((dc->opcode & decinfo[i].mask) == decinfo[i].bits) {
            decinfo[i].dec(dc);
            break;
        }
    }
}

static void mb_tr_init_disas_context(DisasContextBase *dcb, CPUState *cs)
{
    DisasContext *dc = container_of(dcb, DisasContext, base);
    MicroBlazeCPU *cpu = MICROBLAZE_CPU(cs);
    int bound;

    dc->cpu = cpu;
    dc->synced_flags = dc->tb_flags = dc->base.tb->flags;
    dc->delayed_branch = !!(dc->tb_flags & D_FLAG);
    dc->jmp = dc->delayed_branch ? JMP_INDIRECT : JMP_NOJMP;
    dc->cpustate_changed = 0;
    dc->abort_at_next_insn = 0;
    dc->ext_imm = dc->base.tb->cs_base;

    bound = -(dc->base.pc_first | TARGET_PAGE_MASK) / 4;
    dc->base.max_insns = MIN(dc->base.max_insns, bound);
}

static void mb_tr_tb_start(DisasContextBase *dcb, CPUState *cs)
{
}

static void mb_tr_insn_start(DisasContextBase *dcb, CPUState *cs)
{
    tcg_gen_insn_start(dcb->pc_next);
}

static bool mb_tr_breakpoint_check(DisasContextBase *dcb, CPUState *cs,
                                   const CPUBreakpoint *bp)
{
    DisasContext *dc = container_of(dcb, DisasContext, base);

    gen_raise_exception_sync(dc, EXCP_DEBUG);

    /*
     * The address covered by the breakpoint must be included in
     * [tb->pc, tb->pc + tb->size) in order to for it to be
     * properly cleared -- thus we increment the PC here so that
     * the logic setting tb->size below does the right thing.
     */
    dc->base.pc_next += 4;
    return true;
}

static void mb_tr_translate_insn(DisasContextBase *dcb, CPUState *cs)
{
    DisasContext *dc = container_of(dcb, DisasContext, base);
    CPUMBState *env = cs->env_ptr;
    uint32_t ir;

    /* TODO: This should raise an exception, not terminate qemu. */
    if (dc->base.pc_next & 3) {
        cpu_abort(cs, "Microblaze: unaligned PC=%x\n",
                  (uint32_t)dc->base.pc_next);
    }

    dc->clear_imm = 1;
    ir = cpu_ldl_code(env, dc->base.pc_next);
    if (!decode(dc, ir)) {
        old_decode(dc, ir);
    }
    if (dc->clear_imm && (dc->tb_flags & IMM_FLAG)) {
        dc->tb_flags &= ~IMM_FLAG;
        tcg_gen_discard_i32(cpu_imm);
    }
    dc->base.pc_next += 4;

    if (dc->delayed_branch && --dc->delayed_branch == 0) {
        if (dc->tb_flags & DRTI_FLAG) {
            do_rti(dc);
        }
        if (dc->tb_flags & DRTB_FLAG) {
            do_rtb(dc);
        }
        if (dc->tb_flags & DRTE_FLAG) {
            do_rte(dc);
        }
        /* Clear the delay slot flag.  */
        dc->tb_flags &= ~D_FLAG;
        dc->base.is_jmp = DISAS_JUMP;
    }

    /* Force an exit if the per-tb cpu state has changed.  */
    if (dc->base.is_jmp == DISAS_NEXT && dc->cpustate_changed) {
        dc->base.is_jmp = DISAS_UPDATE;
        tcg_gen_movi_i32(cpu_pc, dc->base.pc_next);
    }
}

static void mb_tr_tb_stop(DisasContextBase *dcb, CPUState *cs)
{
    DisasContext *dc = container_of(dcb, DisasContext, base);

    assert(!dc->abort_at_next_insn);

    if (dc->base.is_jmp == DISAS_NORETURN) {
        /* We have already exited the TB. */
        return;
    }

    t_sync_flags(dc);
    if (dc->tb_flags & D_FLAG) {
        sync_jmpstate(dc);
        dc->jmp = JMP_NOJMP;
    }

    switch (dc->base.is_jmp) {
    case DISAS_TOO_MANY:
        assert(dc->jmp == JMP_NOJMP);
        gen_goto_tb(dc, 0, dc->base.pc_next);
        return;

    case DISAS_UPDATE:
        assert(dc->jmp == JMP_NOJMP);
        if (unlikely(cs->singlestep_enabled)) {
            gen_raise_exception(dc, EXCP_DEBUG);
        } else {
            tcg_gen_exit_tb(NULL, 0);
        }
        return;

    case DISAS_JUMP:
        switch (dc->jmp) {
        case JMP_INDIRECT:
            {
                TCGv_i32 tmp_pc = tcg_const_i32(dc->base.pc_next);
                eval_cond_jmp(dc, cpu_btarget, tmp_pc);
                tcg_temp_free_i32(tmp_pc);

                if (unlikely(cs->singlestep_enabled)) {
                    gen_raise_exception(dc, EXCP_DEBUG);
                } else {
                    tcg_gen_exit_tb(NULL, 0);
                }
            }
            return;

        case JMP_DIRECT_CC:
            {
                TCGLabel *l1 = gen_new_label();
                tcg_gen_brcondi_i32(TCG_COND_NE, cpu_btaken, 0, l1);
                gen_goto_tb(dc, 1, dc->base.pc_next);
                gen_set_label(l1);
            }
            /* fall through */

        case JMP_DIRECT:
            gen_goto_tb(dc, 0, dc->jmp_pc);
            return;
        }
        /* fall through */

    default:
        g_assert_not_reached();
    }
}

static void mb_tr_disas_log(const DisasContextBase *dcb, CPUState *cs)
{
    qemu_log("IN: %s\n", lookup_symbol(dcb->pc_first));
    log_target_disas(cs, dcb->pc_first, dcb->tb->size);
}

static const TranslatorOps mb_tr_ops = {
    .init_disas_context = mb_tr_init_disas_context,
    .tb_start           = mb_tr_tb_start,
    .insn_start         = mb_tr_insn_start,
    .breakpoint_check   = mb_tr_breakpoint_check,
    .translate_insn     = mb_tr_translate_insn,
    .tb_stop            = mb_tr_tb_stop,
    .disas_log          = mb_tr_disas_log,
};

void gen_intermediate_code(CPUState *cpu, TranslationBlock *tb, int max_insns)
{
    DisasContext dc;
    translator_loop(&mb_tr_ops, &dc.base, cpu, tb, max_insns);
}

void mb_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    MicroBlazeCPU *cpu = MICROBLAZE_CPU(cs);
    CPUMBState *env = &cpu->env;
    int i;

    if (!env) {
        return;
    }

    qemu_fprintf(f, "IN: PC=%x %s\n",
                 env->pc, lookup_symbol(env->pc));
    qemu_fprintf(f, "rmsr=%x resr=%x rear=%" PRIx64 " "
                 "imm=%x iflags=%x fsr=%x rbtr=%x\n",
                 env->msr, env->esr, env->ear,
                 env->imm, env->iflags, env->fsr, env->btr);
    qemu_fprintf(f, "btaken=%d btarget=%x mode=%s(saved=%s) eip=%d ie=%d\n",
                 env->btaken, env->btarget,
                 (env->msr & MSR_UM) ? "user" : "kernel",
                 (env->msr & MSR_UMS) ? "user" : "kernel",
                 (bool)(env->msr & MSR_EIP),
                 (bool)(env->msr & MSR_IE));
    for (i = 0; i < 12; i++) {
        qemu_fprintf(f, "rpvr%2.2d=%8.8x ", i, env->pvr.regs[i]);
        if ((i + 1) % 4 == 0) {
            qemu_fprintf(f, "\n");
        }
    }

    /* Registers that aren't modeled are reported as 0 */
    qemu_fprintf(f, "redr=%x rpid=0 rzpr=0 rtlbx=0 rtlbsx=0 "
                    "rtlblo=0 rtlbhi=0\n", env->edr);
    qemu_fprintf(f, "slr=%x shr=%x\n", env->slr, env->shr);
    for (i = 0; i < 32; i++) {
        qemu_fprintf(f, "r%2.2d=%8.8x ", i, env->regs[i]);
        if ((i + 1) % 4 == 0)
            qemu_fprintf(f, "\n");
        }
    qemu_fprintf(f, "\n\n");
}

void mb_tcg_init(void)
{
#define R(X)  { &cpu_R[X], offsetof(CPUMBState, regs[X]), "r" #X }
#define SP(X) { &cpu_##X, offsetof(CPUMBState, X), #X }

    static const struct {
        TCGv_i32 *var; int ofs; char name[8];
    } i32s[] = {
        R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
        R(8),  R(9),  R(10), R(11), R(12), R(13), R(14), R(15),
        R(16), R(17), R(18), R(19), R(20), R(21), R(22), R(23),
        R(24), R(25), R(26), R(27), R(28), R(29), R(30), R(31),

        SP(pc),
        SP(msr),
        SP(msr_c),
        SP(imm),
        SP(iflags),
        SP(btaken),
        SP(btarget),
        SP(res_val),
    };

#undef R
#undef SP

    for (int i = 0; i < ARRAY_SIZE(i32s); ++i) {
        *i32s[i].var =
          tcg_global_mem_new_i32(cpu_env, i32s[i].ofs, i32s[i].name);
    }

    cpu_res_addr =
        tcg_global_mem_new(cpu_env, offsetof(CPUMBState, res_addr), "res_addr");
}

void restore_state_to_opc(CPUMBState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->pc = data[0];
}
