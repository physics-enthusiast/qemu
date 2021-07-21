/*
 * LoongArch translate functions
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/* Fixed point arithmetic operation instruction translation */
static bool trans_add_w(DisasContext *ctx, arg_add_w *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0 && a->rk != 0) {
        tcg_gen_add_tl(Rd, Rj, Rk);
        tcg_gen_ext32s_tl(Rd, Rd);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_mov_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_add_d(DisasContext *ctx, arg_add_d *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    check_loongarch_64(ctx);
    if (a->rj != 0 && a->rk != 0) {
        tcg_gen_add_tl(Rd, Rj, Rk);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_mov_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_sub_w(DisasContext *ctx, arg_sub_w *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0 && a->rk != 0) {
        tcg_gen_sub_tl(Rd, Rj, Rk);
        tcg_gen_ext32s_tl(Rd, Rd);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_neg_tl(Rd, Rk);
        tcg_gen_ext32s_tl(Rd, Rd);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_sub_d(DisasContext *ctx, arg_sub_d *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    check_loongarch_64(ctx);
    if (a->rj != 0 && a->rk != 0) {
        tcg_gen_sub_tl(Rd, Rj, Rk);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_neg_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_slt(DisasContext *ctx, arg_slt *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);

    tcg_gen_setcond_tl(TCG_COND_LT, Rd, t0, t1);

    return true;
}

static bool trans_sltu(DisasContext *ctx, arg_sltu *a)
{

    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);

    tcg_gen_setcond_tl(TCG_COND_LTU, Rd, t0, t1);

    return true;
}

static bool trans_slti(DisasContext *ctx, arg_slti *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];
    target_ulong uimm = (target_long)(a->si12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    tcg_gen_setcondi_tl(TCG_COND_LT, Rd, t0, uimm);

    return true;
}

static bool trans_sltui(DisasContext *ctx, arg_sltui *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];
    target_ulong uimm = (target_long)(a->si12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    tcg_gen_setcondi_tl(TCG_COND_LTU, Rd, t0, uimm);

    return true;
}

static bool trans_nor(DisasContext *ctx, arg_nor *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0 && a->rk != 0) {
        tcg_gen_nor_tl(Rd, Rj, Rk);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_not_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_not_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, ~((target_ulong)0));
    }

    return true;
}

static bool trans_and(DisasContext *ctx, arg_and *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (likely(a->rj != 0 && a->rk != 0)) {
        tcg_gen_and_tl(Rd, Rj, Rk);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_or(DisasContext *ctx, arg_or *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (likely(a->rj != 0 && a->rk != 0)) {
        tcg_gen_or_tl(Rd, Rj, Rk);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_mov_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_xor(DisasContext *ctx, arg_xor *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    TCGv Rk = cpu_gpr[a->rk];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (likely(a->rj != 0 && a->rk != 0)) {
        tcg_gen_xor_tl(Rd, Rj, Rk);
    } else if (a->rj == 0 && a->rk != 0) {
        tcg_gen_mov_tl(Rd, Rk);
    } else if (a->rj != 0 && a->rk == 0) {
        tcg_gen_mov_tl(Rd, Rj);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_orn(DisasContext *ctx, arg_orn *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rk);

    tcg_gen_not_tl(t0, t0);
    tcg_gen_or_tl(Rd, Rj, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_andn(DisasContext *ctx, arg_andn *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();
    gen_load_gpr(t0, a->rk);

    tcg_gen_not_tl(t0, t0);
    tcg_gen_and_tl(Rd, Rj, t0);

    tcg_temp_free(t0);
    return true;
}

static bool trans_mul_w(DisasContext *ctx, arg_mul_w *a)
{
    TCGv t0, t1;
    TCGv_i32 t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new_i32();
    t3 = tcg_temp_new_i32();

    tcg_gen_trunc_tl_i32(t2, t0);
    tcg_gen_trunc_tl_i32(t3, t1);
    tcg_gen_mul_i32(t2, t2, t3);
    tcg_gen_ext_i32_tl(Rd, t2);

    tcg_temp_free_i32(t2);
    tcg_temp_free_i32(t3);

    return true;
}

static bool trans_mulh_w(DisasContext *ctx, arg_mulh_w *a)
{
    TCGv t0, t1;
    TCGv_i32 t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new_i32();
    t3 = tcg_temp_new_i32();

    tcg_gen_trunc_tl_i32(t2, t0);
    tcg_gen_trunc_tl_i32(t3, t1);
    tcg_gen_muls2_i32(t2, t3, t2, t3);
    tcg_gen_ext_i32_tl(Rd, t3);

    tcg_temp_free_i32(t2);
    tcg_temp_free_i32(t3);

    return true;
}

static bool trans_mulh_wu(DisasContext *ctx, arg_mulh_wu *a)
{
    TCGv t0, t1;
    TCGv_i32 t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new_i32();
    t3 = tcg_temp_new_i32();

    tcg_gen_trunc_tl_i32(t2, t0);
    tcg_gen_trunc_tl_i32(t3, t1);
    tcg_gen_mulu2_i32(t2, t3, t2, t3);
    tcg_gen_ext_i32_tl(Rd, t3);

    tcg_temp_free_i32(t2);
    tcg_temp_free_i32(t3);

    return true;
}

static bool trans_mul_d(DisasContext *ctx, arg_mul_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);

    check_loongarch_64(ctx);
    tcg_gen_mul_i64(Rd, t0, t1);

    return true;
}

static bool trans_mulh_d(DisasContext *ctx, arg_mulh_d *a)
{
    TCGv t0, t1, t2;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new();

    check_loongarch_64(ctx);
    tcg_gen_muls2_i64(t2, Rd, t0, t1);

    tcg_temp_free(t2);

    return true;
}

static bool trans_mulh_du(DisasContext *ctx, arg_mulh_du *a)
{
    TCGv t0, t1, t2;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new();

    check_loongarch_64(ctx);
    tcg_gen_mulu2_i64(t2, Rd, t0, t1);

    tcg_temp_free(t2);

    return true;
}

static bool trans_mulw_d_w(DisasContext *ctx, arg_mulw_d_w *a)
{
    TCGv_i64 t0, t1, t2;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new_i64();
    t1 = tcg_temp_new_i64();
    t2 = tcg_temp_new_i64();

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32s_i64(t0, t0);
    tcg_gen_ext32s_i64(t1, t1);
    tcg_gen_mul_i64(t2, t0, t1);
    tcg_gen_mov_tl(Rd, t2);

    tcg_temp_free_i64(t0);
    tcg_temp_free_i64(t1);
    tcg_temp_free_i64(t2);

    return true;
}

static bool trans_mulw_d_wu(DisasContext *ctx, arg_mulw_d_wu *a)
{
    TCGv_i64 t0, t1, t2;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new_i64();
    t1 = tcg_temp_new_i64();
    t2 = tcg_temp_new_i64();

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32u_i64(t0, t0);
    tcg_gen_ext32u_i64(t1, t1);
    tcg_gen_mul_i64(t2, t0, t1);
    tcg_gen_mov_tl(Rd, t2);

    tcg_temp_free_i64(t0);
    tcg_temp_free_i64(t1);
    tcg_temp_free_i64(t2);

    return true;
}

static bool trans_div_w(DisasContext *ctx, arg_div_w *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    t2 = tcg_temp_new();
    t3 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32s_tl(t0, t0);
    tcg_gen_ext32s_tl(t1, t1);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
    tcg_gen_and_tl(t2, t2, t3);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
    tcg_gen_or_tl(t2, t2, t3);
    tcg_gen_movi_tl(t3, 0);
    tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
    tcg_gen_div_tl(Rd, t0, t1);
    tcg_gen_ext32s_tl(Rd, Rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_mod_w(DisasContext *ctx, arg_mod_w *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    t2 = tcg_temp_new();
    t3 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32s_tl(t0, t0);
    tcg_gen_ext32s_tl(t1, t1);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, INT_MIN);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1);
    tcg_gen_and_tl(t2, t2, t3);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
    tcg_gen_or_tl(t2, t2, t3);
    tcg_gen_movi_tl(t3, 0);
    tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
    tcg_gen_rem_tl(Rd, t0, t1);
    tcg_gen_ext32s_tl(Rd, Rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_div_wu(DisasContext *ctx, arg_div_wu *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    t2 = tcg_const_tl(0);
    t3 = tcg_const_tl(1);

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32u_tl(t0, t0);
    tcg_gen_ext32u_tl(t1, t1);
    tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
    tcg_gen_divu_tl(Rd, t0, t1);
    tcg_gen_ext32s_tl(Rd, Rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_mod_wu(DisasContext *ctx, arg_mod_wu *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();
    t2 = tcg_const_tl(0);
    t3 = tcg_const_tl(1);

    gen_load_gpr(t0, a->rj);
    gen_load_gpr(t1, a->rk);

    tcg_gen_ext32u_tl(t0, t0);
    tcg_gen_ext32u_tl(t1, t1);
    tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
    tcg_gen_remu_tl(Rd, t0, t1);
    tcg_gen_ext32s_tl(Rd, Rd);

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_div_d(DisasContext *ctx, arg_div_d *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new();
    t3 = tcg_temp_new();

    check_loongarch_64(ctx);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
    tcg_gen_and_tl(t2, t2, t3);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
    tcg_gen_or_tl(t2, t2, t3);
    tcg_gen_movi_tl(t3, 0);
    tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
    tcg_gen_div_tl(Rd, t0, t1);

    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_mod_d(DisasContext *ctx, arg_mod_d *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_temp_new();
    t3 = tcg_temp_new();

    check_loongarch_64(ctx);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t2, t0, -1LL << 63);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, -1LL);
    tcg_gen_and_tl(t2, t2, t3);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, t1, 0);
    tcg_gen_or_tl(t2, t2, t3);
    tcg_gen_movi_tl(t3, 0);
    tcg_gen_movcond_tl(TCG_COND_NE, t1, t2, t3, t2, t1);
    tcg_gen_rem_tl(Rd, t0, t1);

    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_div_du(DisasContext *ctx, arg_div_du *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_const_tl(0);
    t3 = tcg_const_tl(1);

    check_loongarch_64(ctx);
    tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
    tcg_gen_divu_i64(Rd, t0, t1);

    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_mod_du(DisasContext *ctx, arg_mod_du *a)
{
    TCGv t0, t1, t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);
    t1 = get_gpr(a->rk);
    t2 = tcg_const_tl(0);
    t3 = tcg_const_tl(1);

    check_loongarch_64(ctx);
    tcg_gen_movcond_tl(TCG_COND_EQ, t1, t1, t2, t3, t1);
    tcg_gen_remu_i64(Rd, t0, t1);

    tcg_temp_free(t2);
    tcg_temp_free(t3);

    return true;
}

static bool trans_alsl_w(DisasContext *ctx, arg_alsl_w *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rk);

    gen_load_gpr(t0, a->rj);

    tcg_gen_shli_tl(t0, t0, a->sa2 + 1);
    tcg_gen_add_tl(Rd, t0, t1);
    tcg_gen_ext32s_tl(Rd, Rd);

    tcg_temp_free(t0);

    return true;
}

static bool trans_alsl_wu(DisasContext *ctx, arg_alsl_wu *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rk);

    gen_load_gpr(t0, a->rj);

    tcg_gen_shli_tl(t0, t0, a->sa2 + 1);
    tcg_gen_add_tl(t0, t0, t1);
    tcg_gen_ext32u_tl(Rd, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_alsl_d(DisasContext *ctx, arg_alsl_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rk);

    gen_load_gpr(t0, a->rj);

    check_loongarch_64(ctx);
    tcg_gen_shli_tl(t0, t0, a->sa2 + 1);
    tcg_gen_add_tl(Rd, t0, t1);

    tcg_temp_free(t0);

    return true;
}

static bool trans_lu12i_w(DisasContext *ctx, arg_lu12i_w *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    tcg_gen_movi_tl(Rd, a->si20 << 12);

    return true;
}

static bool trans_lu32i_d(DisasContext *ctx, arg_lu32i_d *a)
{
    TCGv_i64 t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new_i64();
    t1 = tcg_temp_new_i64();

    tcg_gen_movi_tl(t0, a->si20);
    tcg_gen_concat_tl_i64(t1, Rd, t0);
    tcg_gen_mov_tl(Rd, t1);

    tcg_temp_free(t0);
    tcg_temp_free(t1);

    return true;
}

static bool trans_lu52i_d(DisasContext *ctx, arg_lu52i_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();

    gen_load_gpr(t1, a->rj);

    tcg_gen_movi_tl(t0, a->si12);
    tcg_gen_shli_tl(t0, t0, 52);
    tcg_gen_andi_tl(t1, t1, 0xfffffffffffffU);
    tcg_gen_or_tl(Rd, t0, t1);

    tcg_temp_free(t0);
    tcg_temp_free(t1);

    return true;
}

static bool trans_pcaddi(DisasContext *ctx, arg_pcaddi *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + (a->si20 << 2);
    tcg_gen_movi_tl(Rd, addr);

    return true;
}

static bool trans_pcalau12i(DisasContext *ctx, arg_pcalau12i *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = (pc + (a->si20 << 12)) & ~0xfff;
    tcg_gen_movi_tl(Rd, addr);

    return true;
}

static bool trans_pcaddu12i(DisasContext *ctx, arg_pcaddu12i *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + (a->si20 << 12);
    tcg_gen_movi_tl(Rd, addr);

    return true;
}

static bool trans_pcaddu18i(DisasContext *ctx, arg_pcaddu18i *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    target_ulong pc = ctx->base.pc_next;
    target_ulong addr = pc + ((target_ulong)(a->si20) << 18);
    tcg_gen_movi_tl(Rd, addr);

    return true;
}

static bool trans_addi_w(DisasContext *ctx, arg_addi_w *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    target_ulong uimm = (target_long)(a->si12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0) {
        tcg_gen_addi_tl(Rd, Rj, uimm);
        tcg_gen_ext32s_tl(Rd, Rd);
    } else {
        tcg_gen_movi_tl(Rd, uimm);
    }

    return true;
}

static bool trans_addi_d(DisasContext *ctx, arg_addi_d *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];
    target_ulong uimm = (target_long)(a->si12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    check_loongarch_64(ctx);
    if (a->rj != 0) {
        tcg_gen_addi_tl(Rd, Rj, uimm);
    } else {
        tcg_gen_movi_tl(Rd, uimm);
    }

    return true;
}

static bool trans_addu16i_d(DisasContext *ctx, arg_addu16i_d *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0) {
        tcg_gen_addi_tl(Rd, Rj, a->si16 << 16);
    } else {
        tcg_gen_movi_tl(Rd, a->si16 << 16);
    }
    return true;
}

static bool trans_andi(DisasContext *ctx, arg_andi *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    target_ulong uimm = (uint16_t)(a->ui12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (likely(a->rj != 0)) {
        tcg_gen_andi_tl(Rd, Rj, uimm);
    } else {
        tcg_gen_movi_tl(Rd, 0);
    }

    return true;
}

static bool trans_ori(DisasContext *ctx, arg_ori *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    target_ulong uimm = (uint16_t)(a->ui12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (a->rj != 0) {
        tcg_gen_ori_tl(Rd, Rj, uimm);
    } else {
        tcg_gen_movi_tl(Rd, uimm);
    }

    return true;
}

static bool trans_xori(DisasContext *ctx, arg_xori *a)
{
    TCGv Rd = cpu_gpr[a->rd];
    TCGv Rj = cpu_gpr[a->rj];

    target_ulong uimm = (uint16_t)(a->ui12);

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    if (likely(a->rj != 0)) {
        tcg_gen_xori_tl(Rd, Rj, uimm);
    } else {
        tcg_gen_movi_tl(Rd, uimm);
    }

    return true;
}

/* Fixed point shift operation instruction translation */
static bool trans_sll_w(DisasContext *ctx, arg_sll_w *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    tcg_gen_andi_tl(t0, t0, 0x1f);
    tcg_gen_shl_tl(t0, t1, t0);
    tcg_gen_ext32s_tl(Rd, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_srl_w(DisasContext *ctx, arg_srl_w *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = tcg_temp_new();

    gen_load_gpr(t0, a->rk);
    gen_load_gpr(t1, a->rj);

    tcg_gen_ext32u_tl(t1, t1);
    tcg_gen_andi_tl(t0, t0, 0x1f);
    tcg_gen_shr_tl(t0, t1, t0);
    tcg_gen_ext32s_tl(Rd, t0);

    tcg_temp_free(t0);
    tcg_temp_free(t1);

    return true;
}

static bool trans_sra_w(DisasContext *ctx, arg_sra_w *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    tcg_gen_andi_tl(t0, t0, 0x1f);
    tcg_gen_sar_tl(Rd, t1, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_sll_d(DisasContext *ctx, arg_sll_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    check_loongarch_64(ctx);
    tcg_gen_andi_tl(t0, t0, 0x3f);
    tcg_gen_shl_tl(Rd, t1, t0);

    tcg_temp_free(t0);

    return true;
}
static bool trans_srl_d(DisasContext *ctx, arg_srl_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    check_loongarch_64(ctx);
    tcg_gen_andi_tl(t0, t0, 0x3f);
    tcg_gen_shr_tl(Rd, t1, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_sra_d(DisasContext *ctx, arg_sra_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    check_loongarch_64(ctx);
    tcg_gen_andi_tl(t0, t0, 0x3f);
    tcg_gen_sar_tl(Rd, t1, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_rotr_w(DisasContext *ctx, arg_rotr_w *a)
{
    TCGv t0, t1;
    TCGv_i32 t2, t3;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rk);
    t1 = get_gpr(a->rj);
    t2 = tcg_temp_new_i32();
    t3 = tcg_temp_new_i32();

    tcg_gen_trunc_tl_i32(t2, t0);
    tcg_gen_trunc_tl_i32(t3, t1);
    tcg_gen_andi_i32(t2, t2, 0x1f);
    tcg_gen_rotr_i32(t2, t3, t2);
    tcg_gen_ext_i32_tl(Rd, t2);

    tcg_temp_free_i32(t2);
    tcg_temp_free_i32(t3);

    return true;
}

static bool trans_rotr_d(DisasContext *ctx, arg_rotr_d *a)
{
    TCGv t0, t1;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();
    t1 = get_gpr(a->rj);

    gen_load_gpr(t0, a->rk);

    check_loongarch_64(ctx);
    tcg_gen_andi_tl(t0, t0, 0x3f);
    tcg_gen_rotr_tl(Rd, t1, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_slli_w(DisasContext *ctx, arg_slli_w *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    tcg_gen_shli_tl(t0, t0, a->ui5);
    tcg_gen_ext32s_tl(Rd, t0);

    tcg_temp_free(t0);

    return true;
}

static bool trans_slli_d(DisasContext *ctx, arg_slli_d *a)
{
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    TCGv t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);
    tcg_gen_shli_tl(Rd, t0, a->ui6);

    tcg_temp_free(t0);

    return true;
}

static bool trans_srli_w(DisasContext *ctx, arg_srli_w *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];
    target_ulong uimm = ((uint16_t)(a->ui5)) & 0x1f;

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = tcg_temp_new();

    gen_load_gpr(t0, a->rj);

    if (uimm != 0) {
        tcg_gen_ext32u_tl(t0, t0);
        tcg_gen_shri_tl(Rd, t0, uimm);
    } else {
        tcg_gen_ext32s_tl(Rd, t0);
    }

    tcg_temp_free(t0);

    return true;
}

static bool trans_srli_d(DisasContext *ctx, arg_srli_d *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    tcg_gen_shri_tl(Rd, t0, a->ui6);

    return true;
}

static bool trans_srai_w(DisasContext *ctx, arg_srai_w *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];
    target_ulong uimm = ((uint16_t)(a->ui5)) & 0x1f;

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    tcg_gen_sari_tl(Rd, t0, uimm);

    return true;
}

static bool trans_srai_d(DisasContext *ctx, arg_srai_d *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    check_loongarch_64(ctx);
    tcg_gen_sari_tl(Rd, t0, a->ui6);

    return true;
}

static bool trans_rotri_w(DisasContext *ctx, arg_rotri_w *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];
    target_ulong uimm = ((uint16_t)(a->ui5)) & 0x1f;

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    if (uimm != 0) {
        TCGv_i32 t1 = tcg_temp_new_i32();

        tcg_gen_trunc_tl_i32(t1, t0);
        tcg_gen_rotri_i32(t1, t1, uimm);
        tcg_gen_ext_i32_tl(Rd, t1);

        tcg_temp_free_i32(t1);
    } else {
        tcg_gen_ext32s_tl(Rd, t0);
    }

    return true;
}

static bool trans_rotri_d(DisasContext *ctx, arg_rotri_d *a)
{
    TCGv t0;
    TCGv Rd = cpu_gpr[a->rd];

    if (a->rd == 0) {
        /* Nop */
        return true;
    }

    t0 = get_gpr(a->rj);

    check_loongarch_64(ctx);
    tcg_gen_rotri_tl(Rd, t0, a->ui6);

    return true;
}
