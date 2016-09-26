/*
 * RISC-V FPU Emulation Helpers for QEMU.
 *
 * Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
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
#include <stdlib.h>
#include "cpu.h"
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"

/* convert RISC-V rounding mode to IEEE library numbers */
unsigned int ieee_rm[] = {
    float_round_nearest_even,
    float_round_to_zero,
    float_round_down,
    float_round_up,
    float_round_ties_away
};

/* obtain rm value to use in computation
 * as the last step, convert rm codes to what the softfloat library expects
 * Adapted from Spike's decode.h:RM
 */
#define RM ({                                             \
if (rm == 7) {                                            \
    rm = env->csr[CSR_FRM];                               \
}                                                         \
if (rm > 4) {                                             \
    helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST); \
}                                                         \
ieee_rm[rm]; })

/* convert softfloat library flag numbers to RISC-V */
unsigned int softfloat_flags_to_riscv(unsigned int flag)
{
    switch (flag) {
    case float_flag_inexact:
        return 1;
    case float_flag_underflow:
        return 2;
    case float_flag_overflow:
        return 4;
    case float_flag_divbyzero:
        return 8;
    case float_flag_invalid:
        return 16;
    default:
        return 0;
    }
}

/* adapted from Spike's decode.h:set_fp_exceptions */
#define set_fp_exceptions() do { \
    env->csr[CSR_FFLAGS] |= softfloat_flags_to_riscv(get_float_exception_flags(\
                            &env->fp_status)); \
    set_float_exception_flags(0, &env->fp_status); \
} while (0)

uint64_t helper_fmadd_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                        uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fmadd_d(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                        uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fmsub_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                        uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1, frs2, frs3 ^ (uint32_t)INT32_MIN, 0,
                          &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fmsub_d(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                        uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1, frs2, frs3 ^ (uint64_t)INT64_MIN, 0,
                          &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fnmsub_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                         uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1 ^ (uint32_t)INT32_MIN, frs2, frs3, 0,
                          &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fnmsub_d(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                         uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1 ^ (uint64_t)INT64_MIN, frs2, frs3, 0,
                          &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fnmadd_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                         uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1 ^ (uint32_t)INT32_MIN, frs2,
                          frs3 ^ (uint32_t)INT32_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fnmadd_d(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                         uint64_t frs3, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1 ^ (uint64_t)INT64_MIN, frs2,
                          frs3 ^ (uint64_t)INT64_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fadd_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                       uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_add(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fsub_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                       uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_sub(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fmul_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                       uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_mul(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fdiv_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2,
                       uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_div(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fsgnj_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = (frs1 & ~(uint32_t)INT32_MIN) | (frs2 & (uint32_t)INT32_MIN);
    return frs1;
}

uint64_t helper_fsgnjn_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = (frs1 & ~(uint32_t)INT32_MIN) | ((~frs2) & (uint32_t)INT32_MIN);
    return frs1;
}

uint64_t helper_fsgnjx_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = frs1 ^ (frs2 & (uint32_t)INT32_MIN);
    return frs1;
}

uint64_t helper_fmin_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = float32_is_any_nan(frs2) ||
           float32_lt_quiet(frs1, frs2, &env->fp_status) ? frs1 : frs2;
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fmax_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = float32_is_any_nan(frs2) ||
           float32_le_quiet(frs2, frs1, &env->fp_status) ? frs1 : frs2;
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fsqrt_s(CPURISCVState *env, uint64_t frs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_sqrt(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fle_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = float32_le(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_flt_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = float32_lt(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_feq_s(CPURISCVState *env, uint64_t frs1, uint64_t frs2)
{
    frs1 = float32_eq(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_w_s(CPURISCVState *env, uint64_t frs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int64_t)((int32_t)float32_to_int32(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_wu_s(CPURISCVState *env, uint64_t frs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int64_t)((int32_t)float32_to_uint32(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

#if defined(TARGET_RISCV64)
uint64_t helper_fcvt_l_s(CPURISCVState *env, uint64_t frs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int64(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_lu_s(CPURISCVState *env, uint64_t frs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_uint64(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}
#endif

uint64_t helper_fcvt_s_w(CPURISCVState *env, target_ulong rs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int32_to_float32((int32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}

uint64_t helper_fcvt_s_wu(CPURISCVState *env, target_ulong rs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint32_to_float32((uint32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}

#if defined(TARGET_RISCV64)
uint64_t helper_fcvt_s_l(CPURISCVState *env, uint64_t rs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int64_to_float32(rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}

uint64_t helper_fcvt_s_lu(CPURISCVState *env, uint64_t rs1, uint64_t rm)
{
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint64_to_float32(rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}
#endif

/* adapted from spike */
#define isNaNF32UI(ui) (0xFF000000 < (uint32_t)((uint_fast32_t)ui << 1))
#define signF32UI(a) ((bool)((uint32_t)a >> 31))
#define expF32UI(a) ((int_fast16_t)(a >> 23) & 0xFF)
#define fracF32UI(a) (a & 0x007FFFFF)

union ui32_f32 { uint32_t ui; uint32_t f; };

uint_fast16_t float32_classify(uint32_t a, float_status *status)
{
    union ui32_f32 uA;
    uint_fast32_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF32UI(uiA) == 0xFF;
    uint_fast16_t subnormalOrZero = expF32UI(uiA) == 0;
    bool sign = signF32UI(uiA);

    return
        (sign && infOrNaN && fracF32UI(uiA) == 0)           << 0 |
        (sign && !infOrNaN && !subnormalOrZero)             << 1 |
        (sign && subnormalOrZero && fracF32UI(uiA))         << 2 |
        (sign && subnormalOrZero && fracF32UI(uiA) == 0)    << 3 |
        (!sign && infOrNaN && fracF32UI(uiA) == 0)          << 7 |
        (!sign && !infOrNaN && !subnormalOrZero)            << 6 |
        (!sign && subnormalOrZero && fracF32UI(uiA))        << 5 |
        (!sign && subnormalOrZero && fracF32UI(uiA) == 0)   << 4 |
        (isNaNF32UI(uiA) &&  float32_is_signaling_nan(uiA, status)) << 8 |
        (isNaNF32UI(uiA) && !float32_is_signaling_nan(uiA, status)) << 9;
}

target_ulong helper_fclass_s(CPURISCVState *env, uint64_t frs1)
{
    frs1 = float32_classify(frs1, &env->fp_status);
    return frs1;
}
