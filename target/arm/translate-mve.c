/*
 *  ARM translation: M-profile MVE instructions
 *
 *  Copyright (c) 2021 Linaro, Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "tcg/tcg-op.h"
#include "tcg/tcg-op-gvec.h"
#include "exec/exec-all.h"
#include "exec/gen-icount.h"
#include "translate.h"
#include "translate-a32.h"

/* Include the generated decoder */
#include "decode-mve.c.inc"

typedef void MVEGenLdStFn(TCGv_ptr, TCGv_ptr, TCGv_i32);

/* Return the offset of a Qn register (same semantics as aa32_vfp_qreg()) */
static inline long mve_qreg_offset(unsigned reg)
{
    return offsetof(CPUARMState, vfp.zregs[reg].d[0]);
}

static TCGv_ptr mve_qreg_ptr(unsigned reg)
{
    TCGv_ptr ret = tcg_temp_new_ptr();
    tcg_gen_addi_ptr(ret, cpu_env, mve_qreg_offset(reg));
    return ret;
}

static bool mve_check_qreg_bank(DisasContext *s, int qmask)
{
    /*
     * Check whether Qregs are in range. For v8.1M only Q0..Q7
     * are supported, see VFPSmallRegisterBank().
     */
    return qmask < 8;
}

static bool mve_eci_check(DisasContext *s)
{
    /*
     * This is a beatwise insn: check that ECI is valid (not a
     * reserved value) and note that we are handling it.
     * Return true if OK, false if we generated an exception.
     */
    s->eci_handled = true;
    switch (s->eci) {
    case ECI_NONE:
    case ECI_A0:
    case ECI_A0A1:
    case ECI_A0A1A2:
    case ECI_A0A1A2B0:
        return true;
    default:
        /* Reserved value: INVSTATE UsageFault */
        gen_exception_insn(s, s->pc_curr, EXCP_INVSTATE, syn_uncategorized(),
                           default_exception_el(s));
        return false;
    }
}

static void mve_update_eci(DisasContext *s)
{
    /*
     * The helper function will always update the CPUState field,
     * so we only need to update the DisasContext field.
     */
    if (s->eci) {
        s->eci = (s->eci == ECI_A0A1A2B0) ? ECI_A0 : ECI_NONE;
    }
}

static bool do_ldst(DisasContext *s, arg_VLDR_VSTR *a, MVEGenLdStFn *fn)
{
    TCGv_i32 addr;
    uint32_t offset;
    TCGv_ptr qreg;

    if (!dc_isar_feature(aa32_mve, s) ||
        !mve_check_qreg_bank(s, a->qd) ||
        !fn) {
        return false;
    }

    /* CONSTRAINED UNPREDICTABLE: we choose to UNDEF */
    if (a->rn == 15 || (a->rn == 13 && a->w)) {
        return false;
    }

    if (!mve_eci_check(s) || !vfp_access_check(s)) {
        return true;
    }

    offset = a->imm << a->size;
    if (!a->a) {
        offset = -offset;
    }
    addr = load_reg(s, a->rn);
    if (a->p) {
        tcg_gen_addi_i32(addr, addr, offset);
    }

    qreg = mve_qreg_ptr(a->qd);
    fn(cpu_env, qreg, addr);
    tcg_temp_free_ptr(qreg);

    /*
     * Writeback always happens after the last beat of the insn,
     * regardless of predication
     */
    if (a->w) {
        if (!a->p) {
            tcg_gen_addi_i32(addr, addr, offset);
        }
        store_reg(s, a->rn, addr);
    } else {
        tcg_temp_free_i32(addr);
    }
    mve_update_eci(s);
    return true;
}

static bool trans_VLDR_VSTR(DisasContext *s, arg_VLDR_VSTR *a)
{
    static MVEGenLdStFn * const ldstfns[4][2] = {
        { gen_helper_mve_vstrb, gen_helper_mve_vldrb },
        { gen_helper_mve_vstrh, gen_helper_mve_vldrh },
        { gen_helper_mve_vstrw, gen_helper_mve_vldrw },
        { NULL, NULL }
    };
    return do_ldst(s, a, ldstfns[a->size][a->l]);
}

#define DO_VLDST_WIDE_NARROW(OP, SLD, ULD, ST)                  \
    static bool trans_##OP(DisasContext *s, arg_VLDR_VSTR *a)   \
    {                                                           \
        static MVEGenLdStFn * const ldstfns[2][2] = {           \
            { gen_helper_mve_##ST, gen_helper_mve_##SLD },      \
            { NULL, gen_helper_mve_##ULD },                     \
        };                                                      \
        return do_ldst(s, a, ldstfns[a->u][a->l]);              \
    }

DO_VLDST_WIDE_NARROW(VLDSTB_H, vldrb_sh, vldrb_uh, vstrb_h)
DO_VLDST_WIDE_NARROW(VLDSTB_W, vldrb_sw, vldrb_uw, vstrb_w)
DO_VLDST_WIDE_NARROW(VLDSTH_W, vldrh_sw, vldrh_uw, vstrh_w)
