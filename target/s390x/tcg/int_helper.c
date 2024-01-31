/*
 *  S/390 integer helper routines
 *
 *  Copyright (c) 2009 Ulrich Hecht
 *  Copyright (c) 2009 Alexander Graf
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
#include "cpu.h"
#include "s390x-internal.h"
#include "tcg_s390x.h"
#include "exec/exec-all.h"
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"

/* #define DEBUG_HELPER */
#ifdef DEBUG_HELPER
#define HELPER_LOG(x...) qemu_log(x)
#else
#define HELPER_LOG(x...)
#endif

/* 64/32 -> 32 signed division */
uint64_t HELPER(divs32)(CPUS390XState *env, int64_t a, int64_t b64)
{
    int32_t b = b64;
    int64_t q, r;

    if (b == 0) {
        tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
    }

    q = a / b;
    r = a % b;

    /* Catch non-representable quotient.  */
    if (q != (int32_t)q) {
        tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
    }

    return deposit64(q, 32, 32, r);
}

/* 64/32 -> 32 unsigned division */
uint64_t HELPER(divu32)(CPUS390XState *env, uint64_t a, uint64_t b64)
{
    uint32_t b = b64;
    uint64_t q, r;

    if (b == 0) {
        tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
    }

    q = a / b;
    r = a % b;

    /* Catch non-representable quotient.  */
    if (q != (uint32_t)q) {
        tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
    }

    return deposit64(q, 32, 32, r);
}

/* 64/64 -> 64 signed division */
Int128 HELPER(divs64)(CPUS390XState *env, int64_t a, int64_t b)
{
    /* Catch divide by zero, and non-representable quotient (MIN / -1).  */
    if (b == 0 || (b == -1 && a == (1ll << 63))) {
        tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
    }
    return int128_make128(a / b, a % b);
}

/* 128 -> 64/64 unsigned division */
Int128 HELPER(divu64)(CPUS390XState *env, uint64_t ah, uint64_t al, uint64_t b)
{
    if (b != 0) {
        uint64_t r = divu128(&al, &ah, b);
        if (ah == 0) {
            return int128_make128(al, r);
        }
    }
    /* divide by zero or overflow */
    tcg_s390_program_interrupt(env, PGM_FIXPT_DIVIDE, GETPC());
}

uint64_t HELPER(cvb)(CPUS390XState *env, uint64_t src, uint32_t n)
{
    int64_t dec, sign = 0, digit, val = 0, pow10 = 0;
    const uintptr_t ra = GETPC();
    uint64_t tmpsrc;
    int i, j;

    for (i = 0; i < n; i++) {
        tmpsrc = wrap_address(env, src + (n - i - 1) * 8);
        dec = cpu_ldq_data_ra(env, tmpsrc, ra);
        for (j = 0; j < 16; j++, dec >>= 4) {
            if (i == 0 && j == 0) {
                sign = dec & 0xf;
                if (sign < 0xa) {
                    tcg_s390_data_exception(env, 0, ra);
                }
                continue;
            }
            digit = dec & 0xf;
            if (digit > 0x9) {
                tcg_s390_data_exception(env, 0, ra);
            }
            if (i == 0 && j == 1) {
                if (sign == 0xb || sign == 0xd) {
                    val = -digit;
                    pow10 = -10;
                } else {
                    val = digit;
                    pow10 = 10;
                }
            } else {
                val += digit * pow10;
                pow10 *= 10;
            }
        }
    }
    return val;
}

uint64_t HELPER(cvd)(int32_t reg)
{
    /* positive 0 */
    uint64_t dec = 0x0c;
    int64_t bin = reg;
    int shift;

    if (bin < 0) {
        bin = -bin;
        dec = 0x0d;
    }

    for (shift = 4; (shift < 64) && bin; shift += 4) {
        dec |= (bin % 10) << shift;
        bin /= 10;
    }

    return dec;
}

Int128 HELPER(cvdg)(int64_t reg)
{
    /* positive 0 */
    Int128 dec = int128_make64(0x0c);
    Int128 bin = int128_makes64(reg);
    Int128 base = int128_make64(10);
    int shift;

    if (!int128_nonneg(bin)) {
        bin = int128_neg(bin);
        dec = int128_make64(0x0d);
    }

    for (shift = 4; (shift < 128) && int128_nz(bin); shift += 4) {
        dec = int128_or(dec, int128_lshift(int128_remu(bin, base), shift));
        bin = int128_divu(bin, base);
    }

    return dec;
}

uint64_t HELPER(popcnt)(uint64_t val)
{
    /* Note that we don't fold past bytes. */
    val = (val & 0x5555555555555555ULL) + ((val >> 1) & 0x5555555555555555ULL);
    val = (val & 0x3333333333333333ULL) + ((val >> 2) & 0x3333333333333333ULL);
    val = (val + (val >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    return val;
}
