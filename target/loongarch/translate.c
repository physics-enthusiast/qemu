/*
 * LoongArch emulation for QEMU - main translation routines.
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "internal.h"
#include "tcg/tcg-op.h"
#include "exec/translator.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "semihosting/semihost.h"

#include "exec/translator.h"
#include "exec/log.h"
#include "qemu/qemu-print.h"
#include "fpu_helper.h"
#include "translate.h"
#include "instmap.h"

/* global register indices */
TCGv cpu_gpr[32], cpu_PC;
TCGv btarget, bcond;
static TCGv cpu_lladdr, cpu_llval;
static TCGv_i32 hflags;
TCGv_i32 fpu_fcsr0;
TCGv_i64 fpu_f64[32];

#include "exec/gen-icount.h"

#define DISAS_STOP       DISAS_TARGET_0
#define DISAS_EXIT       DISAS_TARGET_1

static const char * const regnames[] = {
    "r0", "ra", "tp", "sp", "a0", "a1", "a2", "a3",
    "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3",
    "t4", "t5", "t6", "t7", "t8", "x0", "fp", "s0",
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
};

static const char * const fregnames[] = {
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
};

/* General purpose registers moves. */
void gen_load_gpr(TCGv t, int reg)
{
    if (reg == 0) {
        tcg_gen_movi_tl(t, 0);
    } else {
        tcg_gen_mov_tl(t, cpu_gpr[reg]);
    }
}

void gen_store_gpr(TCGv t, int reg)
{
    if (reg != 0) {
        tcg_gen_mov_tl(cpu_gpr[reg], t);
    }
}

static inline void gen_save_pc(target_ulong pc)
{
    tcg_gen_movi_tl(cpu_PC, pc);
}

static inline void save_cpu_state(DisasContext *ctx, int do_save_pc)
{
    if (do_save_pc && ctx->base.pc_next != ctx->saved_pc) {
        gen_save_pc(ctx->base.pc_next);
        ctx->saved_pc = ctx->base.pc_next;
    }
    if (ctx->hflags != ctx->saved_hflags) {
        tcg_gen_movi_i32(hflags, ctx->hflags);
        ctx->saved_hflags = ctx->hflags;
        switch (ctx->hflags & LOONGARCH_HFLAG_BMASK) {
        case LOONGARCH_HFLAG_BR:
            break;
        case LOONGARCH_HFLAG_BC:
        case LOONGARCH_HFLAG_B:
            tcg_gen_movi_tl(btarget, ctx->btarget);
            break;
        }
    }
}

static inline void restore_cpu_state(CPULoongArchState *env, DisasContext *ctx)
{
    ctx->saved_hflags = ctx->hflags;
    switch (ctx->hflags & LOONGARCH_HFLAG_BMASK) {
    case LOONGARCH_HFLAG_BR:
        break;
    case LOONGARCH_HFLAG_BC:
    case LOONGARCH_HFLAG_B:
        ctx->btarget = env->btarget;
        break;
    }
}

void generate_exception_err(DisasContext *ctx, int excp, int err)
{
    TCGv_i32 texcp = tcg_const_i32(excp);
    TCGv_i32 terr = tcg_const_i32(err);
    save_cpu_state(ctx, 1);
    gen_helper_raise_exception_err(cpu_env, texcp, terr);
    tcg_temp_free_i32(terr);
    tcg_temp_free_i32(texcp);
    ctx->base.is_jmp = DISAS_NORETURN;
}

void generate_exception_end(DisasContext *ctx, int excp)
{
    generate_exception_err(ctx, excp, 0);
}

void gen_reserved_instruction(DisasContext *ctx)
{
    generate_exception_end(ctx, EXCP_INE);
}

void gen_load_fpr32(DisasContext *ctx, TCGv_i32 t, int reg)
{
    tcg_gen_extrl_i64_i32(t, fpu_f64[reg]);
}

void gen_store_fpr32(DisasContext *ctx, TCGv_i32 t, int reg)
{
    TCGv_i64 t64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(t64, t);
    tcg_gen_deposit_i64(fpu_f64[reg], fpu_f64[reg], t64, 0, 32);
    tcg_temp_free_i64(t64);
}

static void gen_load_fpr32h(DisasContext *ctx, TCGv_i32 t, int reg)
{
    tcg_gen_extrh_i64_i32(t, fpu_f64[reg]);
}

static void gen_store_fpr32h(DisasContext *ctx, TCGv_i32 t, int reg)
{
    TCGv_i64 t64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(t64, t);
    tcg_gen_deposit_i64(fpu_f64[reg], fpu_f64[reg], t64, 32, 32);
    tcg_temp_free_i64(t64);
}

void gen_load_fpr64(DisasContext *ctx, TCGv_i64 t, int reg)
{
    tcg_gen_mov_i64(t, fpu_f64[reg]);
}

void gen_store_fpr64(DisasContext *ctx, TCGv_i64 t, int reg)
{
    tcg_gen_mov_i64(fpu_f64[reg], t);
}

/* Addresses computation */
void gen_op_addr_add(DisasContext *ctx, TCGv ret, TCGv arg0, TCGv arg1)
{
    tcg_gen_add_tl(ret, arg0, arg1);
}

/* Sign-extract the low 32-bits to a target_long. */
void gen_move_low32(TCGv ret, TCGv_i64 arg)
{
    tcg_gen_ext32s_i64(ret, arg);
}

/* Sign-extract the high 32-bits to a target_long.  */
void gen_move_high32(TCGv ret, TCGv_i64 arg)
{
    tcg_gen_sari_i64(ret, arg, 32);
}

void check_fpu_enabled(DisasContext *ctx)
{
    /* Nop */
}

/*
 * This code generates a "reserved instruction" exception if 64-bit
 * instructions are not enabled.
 */
void check_loongarch_64(DisasContext *ctx)
{
    if (unlikely(!(ctx->hflags & LOONGARCH_HFLAG_64))) {
        gen_reserved_instruction(ctx);
    }
}

void gen_base_offset_addr(DisasContext *ctx, TCGv addr, int base, int offset)
{
    if (base == 0) {
        tcg_gen_movi_tl(addr, offset);
    } else if (offset == 0) {
        gen_load_gpr(addr, base);
    } else {
        tcg_gen_movi_tl(addr, offset);
        gen_op_addr_add(ctx, addr, cpu_gpr[base], addr);
    }
}


static inline bool use_goto_tb(DisasContext *ctx, target_ulong dest)
{
    return true;
}

static inline void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    if (use_goto_tb(ctx, dest)) {
        tcg_gen_goto_tb(n);
        gen_save_pc(dest);
        tcg_gen_exit_tb(ctx->base.tb, n);
    } else {
        gen_save_pc(dest);
        tcg_gen_lookup_and_goto_ptr();
    }
}

static inline void clear_branch_hflags(DisasContext *ctx)
{
    ctx->hflags &= ~LOONGARCH_HFLAG_BMASK;
    if (ctx->base.is_jmp == DISAS_NEXT) {
        save_cpu_state(ctx, 0);
    } else {
        /*
         * It is not safe to save ctx->hflags as hflags may be changed
         * in execution time.
         */
        tcg_gen_andi_i32(hflags, hflags, ~LOONGARCH_HFLAG_BMASK);
    }
}

static void gen_branch(DisasContext *ctx, int insn_bytes)
{
    if (ctx->hflags & LOONGARCH_HFLAG_BMASK) {
        int proc_hflags = ctx->hflags & LOONGARCH_HFLAG_BMASK;
        /* Branches completion */
        clear_branch_hflags(ctx);
        ctx->base.is_jmp = DISAS_NORETURN;
        switch (proc_hflags & LOONGARCH_HFLAG_BMASK) {
        case LOONGARCH_HFLAG_B:
            /* unconditional branch */
            gen_goto_tb(ctx, 0, ctx->btarget);
            break;
        case LOONGARCH_HFLAG_BC:
            /* Conditional branch */
            {
                TCGLabel *l1 = gen_new_label();

                tcg_gen_brcondi_tl(TCG_COND_NE, bcond, 0, l1);
                gen_goto_tb(ctx, 1, ctx->base.pc_next + insn_bytes);
                gen_set_label(l1);
                gen_goto_tb(ctx, 0, ctx->btarget);
            }
            break;
        case LOONGARCH_HFLAG_BR:
            /* unconditional branch to register */
            tcg_gen_mov_tl(cpu_PC, btarget);
            tcg_gen_lookup_and_goto_ptr();
            break;
        default:
            fprintf(stderr, "unknown branch 0x%x\n", proc_hflags);
            abort();
        }
    }
}

static void loongarch_tr_init_disas_context(DisasContextBase *dcbase,
                                            CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPULoongArchState *env = cs->env_ptr;

    ctx->page_start = ctx->base.pc_first & TARGET_PAGE_MASK;
    ctx->saved_pc = -1;
    ctx->insn_flags = env->insn_flags;
    ctx->btarget = 0;
    ctx->PAMask = env->PAMask;
    /* Restore state from the tb context.  */
    ctx->hflags = (uint32_t)ctx->base.tb->flags;
    restore_cpu_state(env, ctx);
    ctx->mem_idx = LOONGARCH_HFLAG_UM;
    ctx->default_tcg_memop_mask = MO_UNALN;
}

/* loongarch arithmetic */
static void gen_loongarch_arith(DisasContext *ctx, uint32_t opc,
                                int rd, int rj, int rk)
{
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }

    switch (opc) {
    case LA_OPC_ADD_W:
        if (rj != 0 && rk != 0) {
            tcg_gen_add_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_SUB_W:
        if (rj != 0 && rk != 0) {
            tcg_gen_sub_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_neg_tl(cpu_gpr[rd], cpu_gpr[rk]);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_ADD_D:
        if (rj != 0 && rk != 0) {
            tcg_gen_add_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_SUB_D:
        if (rj != 0 && rk != 0) {
            tcg_gen_sub_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_neg_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    }
}

/* loongarch arithmetic with immediate operand */
static void gen_loongarch_arith_imm(DisasContext *ctx, uint32_t opc,
                                    int rd, int rj, int imm)
{
    target_ulong uimm = (target_long)imm;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    switch (opc) {
    case LA_OPC_ADDI_W:
        if (rj != 0) {
            tcg_gen_addi_tl(cpu_gpr[rd], cpu_gpr[rj], uimm);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], uimm);
        }
        break;
    case LA_OPC_ADDI_D:
        if (rj != 0) {
            tcg_gen_addi_tl(cpu_gpr[rd], cpu_gpr[rj], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], uimm);
        }
        break;
    }
}

/* loongarch set on lower than */
static void gen_loongarch_slt(DisasContext *ctx, uint32_t opc,
                              int rd, int rj, int rk)
{
    TCGv t0, t1;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, rj);
    gen_load_gpr(t1, rk);
    switch (opc) {
    case LA_OPC_SLT:
        tcg_gen_setcond_tl(TCG_COND_LT, cpu_gpr[rd], t0, t1);
        break;
    case LA_OPC_SLTU:
        tcg_gen_setcond_tl(TCG_COND_LTU, cpu_gpr[rd], t0, t1);
        break;
    }
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}

/* loongarch set on less than with immediate operand */
static void gen_loongarch_slt_imm(DisasContext *ctx, uint32_t opc,
                                  int rd, int rj, int16_t imm)
{
    target_ulong uimm = (target_long)imm;
    TCGv t0;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    gen_load_gpr(t0, rj);
    switch (opc) {
    case LA_OPC_SLTI:
        tcg_gen_setcondi_tl(TCG_COND_LT, cpu_gpr[rd], t0, uimm);
        break;
    case LA_OPC_SLTIU:
        tcg_gen_setcondi_tl(TCG_COND_LTU, cpu_gpr[rd], t0, uimm);
        break;
    }
    tcg_temp_free(t0);
}

/* loongarch logic */
static void gen_loongarch_logic(DisasContext *ctx, uint32_t opc,
                                int rd, int rj, int rk)
{
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }

    switch (opc) {
    case LA_OPC_AND:
        if (likely(rj != 0 && rk != 0)) {
            tcg_gen_and_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_NOR:
        if (rj != 0 && rk != 0) {
            tcg_gen_nor_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_not_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_not_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], ~((target_ulong)0));
        }
        break;
    case LA_OPC_OR:
        if (likely(rj != 0 && rk != 0)) {
            tcg_gen_or_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_XOR:
        if (likely(rj != 0 && rk != 0)) {
            tcg_gen_xor_tl(cpu_gpr[rd], cpu_gpr[rj], cpu_gpr[rk]);
        } else if (rj == 0 && rk != 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rk]);
        } else if (rj != 0 && rk == 0) {
            tcg_gen_mov_tl(cpu_gpr[rd], cpu_gpr[rj]);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    }
}

/* loongarch logic with immediate operand */
static void gen_loongarch_logic_imm(DisasContext *ctx, uint32_t opc,
                                    int rd, int rj, int16_t imm)
{
    target_ulong uimm;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    uimm = (uint16_t)imm;
    switch (opc) {
    case LA_OPC_ANDI:
        if (likely(rj != 0)) {
            tcg_gen_andi_tl(cpu_gpr[rd], cpu_gpr[rj], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], 0);
        }
        break;
    case LA_OPC_ORI:
        if (rj != 0) {
            tcg_gen_ori_tl(cpu_gpr[rd], cpu_gpr[rj], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], uimm);
        }
        break;
    case LA_OPC_XORI:
        if (likely(rj != 0)) {
            tcg_gen_xori_tl(cpu_gpr[rd], cpu_gpr[rj], uimm);
        } else {
            tcg_gen_movi_tl(cpu_gpr[rd], uimm);
        }
        break;
    default:
        break;
    }
}

/* loongarch mul and div */
static void gen_loongarch_muldiv(DisasContext *ctx, int opc, int rd,
                                 int rj, int rk)
{
    TCGv t0, t1;

    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();

    gen_load_gpr(t0, rj);
    gen_load_gpr(t1, rk);

    switch (opc) {
    case LA_OPC_DIV_W:
        {
            TCGv t2 = tcg_temp_new();
            TCGv t3 = tcg_temp_new();
            tcg_gen_ext32s_tl(t0, t0);
            tcg_gen_ext32s_tl(t1, t1);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
            tcg_gen_and_tl(t2, t2, t3);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
            tcg_gen_or_tl(t2, t2, t3);
            tcg_gen_movi_tl(t3, 0);
            tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
            tcg_gen_div_tl(cpu_gpr[rd], t0, t1);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MOD_W:
        {
            TCGv t2 = tcg_temp_new();
            TCGv t3 = tcg_temp_new();
            tcg_gen_ext32s_tl(t0, t0);
            tcg_gen_ext32s_tl(t1, t1);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
            tcg_gen_and_tl(t2, t2, t3);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
            tcg_gen_or_tl(t2, t2, t3);
            tcg_gen_movi_tl(t3, 0);
            tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
            tcg_gen_rem_tl(cpu_gpr[rd], t0, t1);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_DIV_WU:
        {
            TCGv t2 = tcg_const_tl(0);
            TCGv t3 = tcg_const_tl(1);
            tcg_gen_ext32u_tl(t0, t0);
            tcg_gen_ext32u_tl(t1, t1);
            tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
            tcg_gen_divu_tl(cpu_gpr[rd], t0, t1);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MOD_WU:
        {
            TCGv t2 = tcg_const_tl(0);
            TCGv t3 = tcg_const_tl(1);
            tcg_gen_ext32u_tl(t0, t0);
            tcg_gen_ext32u_tl(t1, t1);
            tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
            tcg_gen_remu_tl(cpu_gpr[rd], t0, t1);
            tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MUL_W:
        {
            TCGv_i32 t2 = tcg_temp_new_i32();
            TCGv_i32 t3 = tcg_temp_new_i32();
            tcg_gen_trunc_tl_i32(t2, t0);
            tcg_gen_trunc_tl_i32(t3, t1);
            tcg_gen_mul_i32(t2, t2, t3);
            tcg_gen_ext_i32_tl(cpu_gpr[rd], t2);
            tcg_temp_free_i32(t2);
            tcg_temp_free_i32(t3);
        }
        break;
    case LA_OPC_MULH_W:
        {
            TCGv_i32 t2 = tcg_temp_new_i32();
            TCGv_i32 t3 = tcg_temp_new_i32();
            tcg_gen_trunc_tl_i32(t2, t0);
            tcg_gen_trunc_tl_i32(t3, t1);
            tcg_gen_muls2_i32(t2, t3, t2, t3);
            tcg_gen_ext_i32_tl(cpu_gpr[rd], t3);
            tcg_temp_free_i32(t2);
            tcg_temp_free_i32(t3);
        }
        break;
    case LA_OPC_MULH_WU:
        {
            TCGv_i32 t2 = tcg_temp_new_i32();
            TCGv_i32 t3 = tcg_temp_new_i32();
            tcg_gen_trunc_tl_i32(t2, t0);
            tcg_gen_trunc_tl_i32(t3, t1);
            tcg_gen_mulu2_i32(t2, t3, t2, t3);
            tcg_gen_ext_i32_tl(cpu_gpr[rd], t3);
            tcg_temp_free_i32(t2);
            tcg_temp_free_i32(t3);
        }
        break;
    case LA_OPC_DIV_D:
        {
            TCGv t2 = tcg_temp_new();
            TCGv t3 = tcg_temp_new();
            tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
            tcg_gen_and_tl(t2, t2, t3);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
            tcg_gen_or_tl(t2, t2, t3);
            tcg_gen_movi_tl(t3, 0);
            tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
            tcg_gen_div_tl(cpu_gpr[rd], t0, t1);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MOD_D:
        {
            TCGv t2 = tcg_temp_new();
            TCGv t3 = tcg_temp_new();
            tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
            tcg_gen_and_tl(t2, t2, t3);
            tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
            tcg_gen_or_tl(t2, t2, t3);
            tcg_gen_movi_tl(t3, 0);
            tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
            tcg_gen_rem_tl(cpu_gpr[rd], t0, t1);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_DIV_DU:
        {
            TCGv t2 = tcg_const_tl(0);
            TCGv t3 = tcg_const_tl(1);
            tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
            tcg_gen_divu_i64(cpu_gpr[rd], t0, t1);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MOD_DU:
        {
            TCGv t2 = tcg_const_tl(0);
            TCGv t3 = tcg_const_tl(1);
            tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
            tcg_gen_remu_i64(cpu_gpr[rd], t0, t1);
            tcg_temp_free(t3);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MUL_D:
        tcg_gen_mul_i64(cpu_gpr[rd], t0, t1);
        break;
    case LA_OPC_MULH_D:
        {
            TCGv t2 = tcg_temp_new();
            tcg_gen_muls2_i64(t2, cpu_gpr[rd], t0, t1);
            tcg_temp_free(t2);
        }
        break;
    case LA_OPC_MULH_DU:
        {
            TCGv t2 = tcg_temp_new();
            tcg_gen_mulu2_i64(t2, cpu_gpr[rd], t0, t1);
            tcg_temp_free(t2);
        }
        break;
    default:
        generate_exception_end(ctx, EXCP_INE);
        goto out;
    }
 out:
    tcg_temp_free(t0);
    tcg_temp_free(t1);

    return;
}

/* loongarch alsl_X */
static void gen_loongarch_alsl(DisasContext *ctx, int opc, int rd,
                               int rj, int rk, int imm2)
{
    TCGv t0;
    TCGv t1;
    if (rd == 0) {
        /* Treat as NOP. */
        return;
    }
    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    gen_load_gpr(t0, rj);
    gen_load_gpr(t1, rk);
    tcg_gen_shli_tl(t0, t0, imm2 + 1);
    tcg_gen_add_tl(cpu_gpr[rd], t0, t1);
    if (opc == LA_OPC_ALSL_W) {
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
    }

    tcg_temp_free(t1);
    tcg_temp_free(t0);

    return;
}

static void loongarch_tr_tb_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void loongarch_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->base.pc_next, ctx->hflags & LOONGARCH_HFLAG_BMASK,
                       ctx->btarget);
}

static bool loongarch_tr_breakpoint_check(DisasContextBase *dcbase,
                                          CPUState *cs,
                                          const CPUBreakpoint *bp)
{
    return true;
}

/* 128 and 256 msa vector instructions are not supported yet */
static bool decode_lsx(uint32_t opcode)
{
    uint32_t value = (opcode & 0xff000000);

    if ((opcode & 0xf0000000) == 0x70000000) {
        return true;
    } else if ((opcode & 0xfff00000) == 0x38400000) {
        return true;
    } else {
        switch (value) {
        case 0x09000000:
        case 0x0a000000:
        case 0x0e000000:
        case 0x0f000000:
        case 0x2c000000:
        case 0x30000000:
        case 0x31000000:
        case 0x32000000:
        case 0x33000000:
            return true;
        }
    }
    return false;
}

#include "decode-insns.c.inc"
#include "trans.inc.c"

static void loongarch_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    CPULoongArchState *env = cs->env_ptr;
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    int insn_bytes = 4;

    ctx->opcode = cpu_ldl_code(env, ctx->base.pc_next);

    if (!decode(ctx, ctx->opcode)) {
        if (!decode_lsx(ctx->opcode)) {
            fprintf(stderr, "Error: unkown opcode. 0x%lx: 0x%x\n",
                    ctx->base.pc_next, ctx->opcode);
        }
        generate_exception_end(ctx, EXCP_INE);
    }

    if (ctx->hflags & LOONGARCH_HFLAG_BMASK) {
        gen_branch(ctx, insn_bytes);
    }
    ctx->base.pc_next += insn_bytes;
}

static void loongarch_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_STOP:
        gen_save_pc(ctx->base.pc_next);
        tcg_gen_lookup_and_goto_ptr();
        break;
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        save_cpu_state(ctx, 0);
        gen_goto_tb(ctx, 0, ctx->base.pc_next);
        break;
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    case DISAS_NORETURN:
        break;
    default:
        g_assert_not_reached();
    }
}

static void loongarch_tr_disas_log(const DisasContextBase *dcbase, CPUState *cs)
{
    qemu_log("IN: %s\n", lookup_symbol(dcbase->pc_first));
    log_target_disas(cs, dcbase->pc_first, dcbase->tb->size);
}

static const TranslatorOps loongarch_tr_ops = {
    .init_disas_context = loongarch_tr_init_disas_context,
    .tb_start           = loongarch_tr_tb_start,
    .insn_start         = loongarch_tr_insn_start,
    .breakpoint_check   = loongarch_tr_breakpoint_check,
    .translate_insn     = loongarch_tr_translate_insn,
    .tb_stop            = loongarch_tr_tb_stop,
    .disas_log          = loongarch_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb, int max_insns)
{
    DisasContext ctx;

    translator_loop(&loongarch_tr_ops, &ctx.base, cs, tb, max_insns);
}

static void fpu_dump_state(CPULoongArchState *env, FILE * f, int flags)
{
    int i;
    int is_fpu64 = 1;

#define printfpr(fp)                                              \
    do {                                                          \
        if (is_fpu64)                                             \
            qemu_fprintf(f, "w:%08x d:%016" PRIx64                \
                        " fd:%13g fs:%13g psu: %13g\n",           \
                        (fp)->w[FP_ENDIAN_IDX], (fp)->d,          \
                        (double)(fp)->fd,                         \
                        (double)(fp)->fs[FP_ENDIAN_IDX],          \
                        (double)(fp)->fs[!FP_ENDIAN_IDX]);        \
        else {                                                    \
            fpr_t tmp;                                            \
            tmp.w[FP_ENDIAN_IDX] = (fp)->w[FP_ENDIAN_IDX];        \
            tmp.w[!FP_ENDIAN_IDX] = ((fp) + 1)->w[FP_ENDIAN_IDX]; \
            qemu_fprintf(f, "w:%08x d:%016" PRIx64                \
                        " fd:%13g fs:%13g psu:%13g\n",            \
                        tmp.w[FP_ENDIAN_IDX], tmp.d,              \
                        (double)tmp.fd,                           \
                        (double)tmp.fs[FP_ENDIAN_IDX],            \
                        (double)tmp.fs[!FP_ENDIAN_IDX]);          \
        }                                                         \
    } while (0)


    qemu_fprintf(f,
                 "FCSR0 0x%08x  SR.FR %d  fp_status 0x%02x\n",
                 env->active_fpu.fcsr0, is_fpu64,
                 get_float_exception_flags(&env->active_fpu.fp_status));
    for (i = 0; i < 32; (is_fpu64) ? i++ : (i += 2)) {
        qemu_fprintf(f, "%3s: ", fregnames[i]);
        printfpr(&env->active_fpu.fpr[i]);
    }

#undef printfpr
}

void loongarch_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    LoongArchCPU *cpu = LOONGARCH_CPU(cs);
    CPULoongArchState *env = &cpu->env;
    int i;

    qemu_fprintf(f, "pc=0x" TARGET_FMT_lx " ds %04x "
                 TARGET_FMT_lx " " TARGET_FMT_ld "\n",
                 env->active_tc.PC, env->hflags, env->btarget, env->bcond);
    for (i = 0; i < 32; i++) {
        if ((i & 3) == 0) {
            qemu_fprintf(f, "GPR%02d:", i);
        }
        qemu_fprintf(f, " %s " TARGET_FMT_lx,
                     regnames[i], env->active_tc.gpr[i]);
        if ((i & 3) == 3) {
            qemu_fprintf(f, "\n");
        }
    }

    qemu_fprintf(f, "EUEN            0x%lx\n", env->CSR_EUEN);
    qemu_fprintf(f, "ESTAT           0x%lx\n", env->CSR_ESTAT);
    qemu_fprintf(f, "ERA             0x%lx\n", env->CSR_ERA);
    qemu_fprintf(f, "CRMD            0x%lx\n", env->CSR_CRMD);
    qemu_fprintf(f, "PRMD            0x%lx\n", env->CSR_PRMD);
    qemu_fprintf(f, "BadVAddr        0x%lx\n", env->CSR_BADV);
    qemu_fprintf(f, "TLB refill ERA  0x%lx\n", env->CSR_TLBRERA);
    qemu_fprintf(f, "TLB refill BadV 0x%lx\n", env->CSR_TLBRBADV);
    qemu_fprintf(f, "EEPN            0x%lx\n", env->CSR_EEPN);
    qemu_fprintf(f, "BadInstr        0x%lx\n", env->CSR_BADI);
    qemu_fprintf(f, "PRCFG1    0x%lx\nPRCFG2     0x%lx\nPRCFG3     0x%lx\n",
                 env->CSR_PRCFG1, env->CSR_PRCFG3, env->CSR_PRCFG3);
    if ((flags & CPU_DUMP_FPU) && (env->hflags & LOONGARCH_HFLAG_FPU)) {
        fpu_dump_state(env, f, flags);
    }
}

void loongarch_tcg_init(void)
{
    int i;

    for (i = 0; i < 32; i++)
        cpu_gpr[i] = tcg_global_mem_new(cpu_env,
                                        offsetof(CPULoongArchState,
                                                 active_tc.gpr[i]),
                                        regnames[i]);

    for (i = 0; i < 32; i++) {
        int off = offsetof(CPULoongArchState, active_fpu.fpr[i].d);
        fpu_f64[i] = tcg_global_mem_new_i64(cpu_env, off, fregnames[i]);
    }

    cpu_PC = tcg_global_mem_new(cpu_env,
                                offsetof(CPULoongArchState,
                                         active_tc.PC), "PC");
    bcond = tcg_global_mem_new(cpu_env,
                               offsetof(CPULoongArchState, bcond), "bcond");
    btarget = tcg_global_mem_new(cpu_env,
                                 offsetof(CPULoongArchState, btarget),
                                 "btarget");
    hflags = tcg_global_mem_new_i32(cpu_env,
                                    offsetof(CPULoongArchState, hflags),
                                    "hflags");
    fpu_fcsr0 = tcg_global_mem_new_i32(cpu_env,
                                   offsetof(CPULoongArchState,
                                            active_fpu.fcsr0), "fcsr0");
    cpu_lladdr = tcg_global_mem_new(cpu_env,
                                    offsetof(CPULoongArchState, lladdr),
                                    "lladdr");
    cpu_llval = tcg_global_mem_new(cpu_env,
                                   offsetof(CPULoongArchState, llval),
                                   "llval");
}

void restore_state_to_opc(CPULoongArchState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->active_tc.PC = data[0];
    env->hflags &= ~LOONGARCH_HFLAG_BMASK;
    env->hflags |= data[1];
    switch (env->hflags & LOONGARCH_HFLAG_BMASK) {
    case LOONGARCH_HFLAG_BR:
        break;
    case LOONGARCH_HFLAG_BC:
    case LOONGARCH_HFLAG_B:
        env->btarget = data[2];
        break;
    }
}
