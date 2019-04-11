/*
 * QEMU TCG support -- s390x vector integer instruction support
 *
 * Copyright (C) 2019 Red Hat Inc
 *
 * Authors:
 *   David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "cpu.h"
#include "vec.h"
#include "exec/helper-proto.h"
#include "tcg/tcg-gvec-desc.h"

/*
 * Add two 128 bit vectors, returning the carry.
 */
static bool s390_vec_add(S390Vector *d, const S390Vector *a,
                         const S390Vector *b)
{
    bool low_carry = false, high_carry = false;

    if (a->doubleword[0] + b->doubleword[0] < a->doubleword[0]) {
        high_carry = true;
    }
    if (a->doubleword[1] + b->doubleword[1] < a->doubleword[1]) {
        low_carry = true;
        if (a->doubleword[0] == b->doubleword[0]) {
            high_carry = true;
        }
    }
    d->doubleword[0] = a->doubleword[0] + b->doubleword[0] + low_carry;
    d->doubleword[1] = a->doubleword[1] + b->doubleword[1];
    return high_carry;
}

/*
 * Subtract two 128 bit vectors, returning the borrow.
 */
static bool s390_vec_sub(S390Vector *d, const S390Vector *a,
                         const S390Vector *b)
{
    bool low_borrow = false, high_borrow = false;

    if (a->doubleword[0] < b->doubleword[0]) {
        high_borrow = true;
    } else if (a->doubleword[1] < b->doubleword[0]) {
        low_borrow = true;
        if (a->doubleword[0] == b->doubleword[0]) {
            high_borrow = true;
        }
    }
    d->doubleword[0] = a->doubleword[0] - b->doubleword[0] - low_borrow;
    d->doubleword[1] = a->doubleword[1] - b->doubleword[1];
    return high_borrow;
}

static bool s390_vec_is_zero(const S390Vector *v)
{
    return !v->doubleword[0] && !v->doubleword[1];
}

static void s390_vec_or(S390Vector *res, const S390Vector *a,
                        const S390Vector *b)
{
    res->doubleword[0] = a->doubleword[0] | b->doubleword[0];
    res->doubleword[1] = a->doubleword[1] | b->doubleword[1];
}

static void s390_vec_xor(S390Vector *res, const S390Vector *a,
                         const S390Vector *b)
{
    res->doubleword[0] = a->doubleword[0] ^ b->doubleword[0];
    res->doubleword[1] = a->doubleword[1] ^ b->doubleword[1];
}

static void s390_vec_shl(S390Vector *d, const S390Vector *a, uint64_t count)
{
    uint64_t tmp;

    g_assert(count < 128);
    if (count == 0) {
        d->doubleword[0] = a->doubleword[0];
        d->doubleword[1] = a->doubleword[1];
    } else if (count == 64) {
        d->doubleword[0] = a->doubleword[1];
        d->doubleword[1] = 0;
    } else if (count < 64) {
        tmp = extract64(a->doubleword[1], 64 - count, count);
        d->doubleword[1] = a->doubleword[1] << count;
        d->doubleword[0] = (a->doubleword[0] << count) | tmp;
    } else {
        d->doubleword[0] = a->doubleword[1] << (count - 64);
        d->doubleword[1] = 0;
    }
}

static void s390_vec_sar(S390Vector *d, const S390Vector *a, uint64_t count)
{
    uint64_t tmp;

    if (count == 0) {
        d->doubleword[0] = a->doubleword[0];
        d->doubleword[1] = a->doubleword[1];
    } else if (count == 64) {
        d->doubleword[1] = a->doubleword[0];
        d->doubleword[0] = 0;
    } else if (count < 64) {
        tmp = a->doubleword[1] >> count;
        d->doubleword[1] = deposit64(tmp, 64 - count, count, a->doubleword[0]);
        d->doubleword[0] = (int64_t)a->doubleword[0] >> count;
    } else {
        d->doubleword[1] = (int64_t)a->doubleword[0] >> (count - 64);
        d->doubleword[0] = 0;
    }
}

static void s390_vec_shr(S390Vector *d, const S390Vector *a, uint64_t count)
{
    uint64_t tmp;

    g_assert(count < 128);
    if (count == 0) {
        d->doubleword[0] = a->doubleword[0];
        d->doubleword[1] = a->doubleword[1];
    } else if (count == 64) {
        d->doubleword[1] = a->doubleword[0];
        d->doubleword[0] = 0;
    } else if (count < 64) {
        tmp = a->doubleword[1] >> count;
        d->doubleword[1] = deposit64(tmp, 64 - count, count, a->doubleword[0]);
        d->doubleword[0] = a->doubleword[0] >> count;
    } else {
        d->doubleword[1] = a->doubleword[0] >> (count - 64);
        d->doubleword[0] = 0;
    }
}

void HELPER(gvec_vacc128)(void *v1, const void *v2, const void *v3,
                          uint32_t desc)
{
    S390Vector tmp, *dst = v1;

    dst->doubleword[0] = 0;
    dst->doubleword[1] = s390_vec_add(&tmp, v2, v3);
}

void HELPER(gvec_vaccc128)(void *v1, const void *v2, const void *v3,
                           const void *v4, uint32_t desc)
{
    const S390Vector old_carry = {
        .doubleword[0] = 0,
        .doubleword[1] = ((S390Vector *)v4)->doubleword[1] & 1,
    };
    S390Vector tmp, *dst = v1;
    bool carry;

    carry = s390_vec_add(&tmp, v2, v3);
    carry |= s390_vec_add(&tmp, &tmp, &old_carry);
    dst->doubleword[0] = 0;
    dst->doubleword[1] = carry;
}

#define DEF_VAVG(BITS)                                                         \
void HELPER(gvec_vavg##BITS)(void *v1, const void *v2, const void *v3,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int32_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, i);   \
        const int32_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, i);   \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a + b + 1) >> 1);                 \
    }                                                                          \
}
DEF_VAVG(8)
DEF_VAVG(16)

#define DEF_VAVGL(BITS)                                                        \
void HELPER(gvec_vavgl##BITS)(void *v1, const void *v2, const void *v3,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a + b + 1) >> 1);                 \
    }                                                                          \
}
DEF_VAVGL(8)
DEF_VAVGL(16)

#define DEF_VCLZ(BITS)                                                         \
void HELPER(gvec_vclz##BITS)(void *v1, const void *v2, uint32_t desc)          \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, clz32(a) - 32 + BITS);             \
    }                                                                          \
}
DEF_VCLZ(8)
DEF_VCLZ(16)

#define DEF_VCTZ(BITS)                                                         \
void HELPER(gvec_vctz##BITS)(void *v1, const void *v2, uint32_t desc)          \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, ctz32(a));                         \
    }                                                                          \
}
DEF_VCTZ(8)
DEF_VCTZ(16)

/* like binary multiplication, but XOR instead of addition */
#define DEF_GALOIS_MULTIPLY(BITS, TBITS)                                       \
static uint##TBITS##_t galois_multiply##BITS(uint##TBITS##_t a,                \
                                             uint##TBITS##_t b)                \
{                                                                              \
    uint##TBITS##_t res = 0;                                                   \
                                                                               \
    while (b) {                                                                \
        if (b & 0x1) {                                                         \
            res = res ^ a;                                                     \
        }                                                                      \
        a = a << 1;                                                            \
        b = b >> 1;                                                            \
    }                                                                          \
    return res;                                                                \
}
DEF_GALOIS_MULTIPLY(8, 16)
DEF_GALOIS_MULTIPLY(16, 32)
DEF_GALOIS_MULTIPLY(32, 64)

static S390Vector galois_multiply64(uint64_t a, uint64_t b)
{
    S390Vector res = {};
    S390Vector va = {
        .doubleword[1] = a,
    };
    S390Vector vb = {
        .doubleword[1] = b,
    };

    while (!s390_vec_is_zero(&vb)) {
        if (vb.doubleword[1] & 0x1) {
            s390_vec_xor(&res, &res, &va);
        }
        s390_vec_shl(&va, &va, 1);
        s390_vec_shr(&vb, &vb, 1);
    }
    return res;
}

#define DEF_VGFM(BITS, TBITS)                                                  \
void HELPER(gvec_vgfm##BITS)(void *v1, const void *v2, const void *v3,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / TBITS); i++) {                                      \
        uint##BITS##_t a = s390_vec_read_element##BITS(v2, i * 2);             \
        uint##BITS##_t b = s390_vec_read_element##BITS(v3, i * 2);             \
        uint##TBITS##_t d = galois_multiply##BITS(a, b);                       \
                                                                               \
        a = s390_vec_read_element##BITS(v2, i * 2 + 1);                        \
        b = s390_vec_read_element##BITS(v3, i * 2 + 1);                        \
        d = d ^ galois_multiply32(a, b);                                       \
        s390_vec_write_element##TBITS(v1, i, d);                               \
    }                                                                          \
}
DEF_VGFM(8, 16)
DEF_VGFM(16, 32)
DEF_VGFM(32, 64)

void HELPER(gvec_vgfm64)(void *v1, const void *v2, const void *v3,
                         uint32_t desc)
{
    S390Vector tmp1, tmp2;
    uint64_t a, b;

    a = s390_vec_read_element64(v2, 0);
    b = s390_vec_read_element64(v3, 0);
    tmp1 = galois_multiply64(a, b);
    a = s390_vec_read_element64(v2, 1);
    b = s390_vec_read_element64(v3, 1);
    tmp2 = galois_multiply64(a, b);
    s390_vec_xor(v1, &tmp1, &tmp2);
}

#define DEF_VGFMA(BITS, TBITS)                                                 \
void HELPER(gvec_vgfma##BITS)(void *v1, const void *v2, const void *v3,        \
                              const void *v4, uint32_t desc)                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / TBITS); i++) {                                      \
        uint##BITS##_t a = s390_vec_read_element##BITS(v2, i * 2);             \
        uint##BITS##_t b = s390_vec_read_element##BITS(v3, i * 2);             \
        uint##TBITS##_t d = galois_multiply##BITS(a, b);                       \
                                                                               \
        a = s390_vec_read_element##BITS(v2, i * 2 + 1);                        \
        b = s390_vec_read_element##BITS(v3, i * 2 + 1);                        \
        d = d ^ galois_multiply32(a, b);                                       \
        d = d ^ s390_vec_read_element##TBITS(v4, i);                           \
        s390_vec_write_element##TBITS(v1, i, d);                               \
    }                                                                          \
}
DEF_VGFMA(8, 16)
DEF_VGFMA(16, 32)
DEF_VGFMA(32, 64)

void HELPER(gvec_vgfma64)(void *v1, const void *v2, const void *v3,
                          const void *v4, uint32_t desc)
{
    S390Vector tmp1, tmp2;
    uint64_t a, b;

    a = s390_vec_read_element64(v2, 0);
    b = s390_vec_read_element64(v3, 0);
    tmp1 = galois_multiply64(a, b);
    a = s390_vec_read_element64(v2, 1);
    b = s390_vec_read_element64(v3, 1);
    tmp2 = galois_multiply64(a, b);
    s390_vec_xor(&tmp1, &tmp1, &tmp2);
    s390_vec_xor(v1, &tmp1, v4);
}

#define DEF_VLP(BITS)                                                          \
void HELPER(gvec_vlp##BITS)(void *v1, const void *v2, uint32_t desc)           \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int##BITS##_t a = s390_vec_read_element##BITS(v2, i);            \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a < 0 ? -a : a);                   \
    }                                                                          \
}
DEF_VLP(8)
DEF_VLP(16)

#define DEF_VMAL(BITS)                                                         \
void HELPER(gvec_vmal##BITS)(void *v1, const void *v2, const void *v3,         \
                             const void *v4, uint32_t desc)                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
        const uint##BITS##_t c = s390_vec_read_element##BITS(v4, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a * b + c);                        \
    }                                                                          \
}
DEF_VMAL(8)
DEF_VMAL(16)

#define DEF_VMAH(BITS)                                                         \
void HELPER(gvec_vmah##BITS)(void *v1, const void *v2, const void *v3,         \
                             const void *v4, uint32_t desc)                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int32_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, i);   \
        const int32_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, i);   \
        const int32_t c = (int##BITS##_t)s390_vec_read_element##BITS(v4, i);   \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a * b + c) >> BITS);              \
    }                                                                          \
}
DEF_VMAH(8)
DEF_VMAH(16)

#define DEF_VMALH(BITS)                                                        \
void HELPER(gvec_vmalh##BITS)(void *v1, const void *v2, const void *v3,        \
                              const void *v4, uint32_t desc)                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
        const uint##BITS##_t c = s390_vec_read_element##BITS(v4, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a * b + c) >> BITS);              \
    }                                                                          \
}
DEF_VMALH(8)
DEF_VMALH(16)

#define DEF_VMAE(BITS, TBITS)                                                  \
void HELPER(gvec_vmae##BITS)(void *v1, const void *v2, const void *v3,         \
                             const void *v4, uint32_t desc)                    \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 0; i < (128 / TBITS); i++, j += 2) {                       \
        int##TBITS##_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, j);  \
        int##TBITS##_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, j);  \
        int##TBITS##_t c = (int##BITS##_t)s390_vec_read_element##BITS(v4, j);  \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b + c);                       \
    }                                                                          \
}
DEF_VMAE(8, 16)
DEF_VMAE(16, 32)
DEF_VMAE(32, 64)

#define DEF_VMALE(BITS, TBITS)                                                 \
void HELPER(gvec_vmale##BITS)(void *v1, const void *v2, const void *v3,        \
                              const void *v4, uint32_t desc)                   \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 0; i < (128 / TBITS); i++, j += 2) {                       \
        uint##TBITS##_t a = s390_vec_read_element##BITS(v2, j);                \
        uint##TBITS##_t b = s390_vec_read_element##BITS(v3, j);                \
        uint##TBITS##_t c = s390_vec_read_element##BITS(v4, j);                \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b + c);                       \
    }                                                                          \
}
DEF_VMALE(8, 16)
DEF_VMALE(16, 32)
DEF_VMALE(32, 64)

#define DEF_VMAO(BITS, TBITS)                                                  \
void HELPER(gvec_vmao##BITS)(void *v1, const void *v2, const void *v3,         \
                             const void *v4, uint32_t desc)                    \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 1; i < (128 / TBITS); i++, j += 2) {                       \
        int##TBITS##_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, j);  \
        int##TBITS##_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, j);  \
        int##TBITS##_t c = (int##BITS##_t)s390_vec_read_element##BITS(v4, j);  \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b + c);                       \
    }                                                                          \
}
DEF_VMAO(8, 16)
DEF_VMAO(16, 32)
DEF_VMAO(32, 64)

#define DEF_VMALO(BITS, TBITS)                                                 \
void HELPER(gvec_vmalo##BITS)(void *v1, const void *v2, const void *v3,        \
                              const void *v4, uint32_t desc)                   \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 1; i < (128 / TBITS); i++, j += 2) {                       \
        uint##TBITS##_t a = s390_vec_read_element##BITS(v2, j);                \
        uint##TBITS##_t b = s390_vec_read_element##BITS(v3, j);                \
        uint##TBITS##_t c = s390_vec_read_element##BITS(v4, j);                \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b + c);                       \
    }                                                                          \
}
DEF_VMALO(8, 16)
DEF_VMALO(16, 32)
DEF_VMALO(32, 64)

#define DEF_VMH(BITS)                                                          \
void HELPER(gvec_vmh##BITS)(void *v1, const void *v2, const void *v3,          \
                            uint32_t desc)                                     \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int32_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, i);   \
        const int32_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, i);   \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a * b) >> BITS);                  \
    }                                                                          \
}
DEF_VMH(8)
DEF_VMH(16)

#define DEF_VMLH(BITS)                                                         \
void HELPER(gvec_vmlh##BITS)(void *v1, const void *v2, const void *v3,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, (a * b) >> BITS);                  \
    }                                                                          \
}
DEF_VMLH(8)
DEF_VMLH(16)

#define DEF_VME(BITS, TBITS)                                                   \
void HELPER(gvec_vme##BITS)(void *v1, const void *v2, const void *v3,          \
                            uint32_t desc)                                     \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 0; i < (128 / TBITS); i++, j += 2) {                       \
        int##TBITS##_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, j);  \
        int##TBITS##_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, j);  \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b);                           \
    }                                                                          \
}
DEF_VME(8, 16)
DEF_VME(16, 32)
DEF_VME(32, 64)

#define DEF_VMLE(BITS, TBITS)                                                  \
void HELPER(gvec_vmle##BITS)(void *v1, const void *v2, const void *v3,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 0; i < (128 / TBITS); i++, j += 2) {                       \
        const uint##TBITS##_t a = s390_vec_read_element##BITS(v2, j);          \
        const uint##TBITS##_t b = s390_vec_read_element##BITS(v3, j);          \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b);                           \
    }                                                                          \
}
DEF_VMLE(8, 16)
DEF_VMLE(16, 32)
DEF_VMLE(32, 64)

#define DEF_VMO(BITS, TBITS)                                                   \
void HELPER(gvec_vmo##BITS)(void *v1, const void *v2, const void *v3,          \
                            uint32_t desc)                                     \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 1; i < (128 / TBITS); i++, j += 2) {                       \
        int##TBITS##_t a = (int##BITS##_t)s390_vec_read_element##BITS(v2, j);  \
        int##TBITS##_t b = (int##BITS##_t)s390_vec_read_element##BITS(v3, j);  \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b);                           \
    }                                                                          \
}
DEF_VMO(8, 16)
DEF_VMO(16, 32)
DEF_VMO(32, 64)

#define DEF_VMLO(BITS, TBITS)                                                  \
void HELPER(gvec_vmlo##BITS)(void *v1, const void *v2, const void *v3,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i, j;                                                                  \
                                                                               \
    for (i = 0, j = 0; i < (128 / TBITS); i++, j += 2) {                       \
        const uint##TBITS##_t a = s390_vec_read_element##BITS(v2, j);          \
        const uint##TBITS##_t b = s390_vec_read_element##BITS(v3, j);          \
                                                                               \
        s390_vec_write_element##TBITS(v1, i, a * b);                           \
    }                                                                          \
}
DEF_VMLO(8, 16)
DEF_VMLO(16, 32)
DEF_VMLO(32, 64)

#define DEF_VPOPCT(BITS)                                                       \
void HELPER(gvec_vpopct##BITS)(void *v1, const void *v2, uint32_t desc)        \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, ctpop32(a));                       \
    }                                                                          \
}
DEF_VPOPCT(8)
DEF_VPOPCT(16)

#define DEF_ROTL(BITS)                                                         \
static uint##BITS##_t rotl##BITS(uint##BITS##_t a, uint8_t count)              \
{                                                                              \
    count &= BITS - 1;                                                         \
    return (a << count) | (a >> (BITS - count));                               \
}
DEF_ROTL(8)
DEF_ROTL(16)

#define DEF_VERLLV(BITS)                                                       \
void HELPER(gvec_verllv##BITS)(void *v1, const void *v2, const void *v3,       \
                               uint32_t desc)                                  \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, rotl##BITS(a, b));                 \
    }                                                                          \
}
DEF_VERLLV(8)
DEF_VERLLV(16)

#define DEF_VERLL(BITS)                                                        \
void HELPER(gvec_verll##BITS)(void *v1, const void *v2, uint64_t count,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, rotl##BITS(a, count));             \
    }                                                                          \
}
DEF_VERLL(8)
DEF_VERLL(16)

#define DEF_VERIM(BITS)                                                        \
void HELPER(gvec_verim##BITS)(void *v1, const void *v2, const void *v3,        \
                              uint32_t desc)                                   \
{                                                                              \
    const uint8_t count = simd_data(desc);                                     \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t mask = s390_vec_read_element##BITS(v3, i);        \
        const uint##BITS##_t d = (a & ~mask) | (rotl##BITS(a, count) & mask);  \
                                                                               \
        s390_vec_write_element##BITS(v1, i, d);                                \
    }                                                                          \
}
DEF_VERIM(8)
DEF_VERIM(16)

#define DEF_VESLV(BITS)                                                        \
void HELPER(gvec_veslv##BITS)(void *v1, const void *v2, const void *v3,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint8_t shift = s390_vec_read_element##BITS(v3, i) & (BITS - 1); \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a << shift);                       \
    }                                                                          \
}
DEF_VESLV(8)
DEF_VESLV(16)
DEF_VESLV(32)
DEF_VESLV(64)

#define DEF_VESRAV(BITS)                                                       \
void HELPER(gvec_vesrav##BITS)(void *v1, const void *v2, const void *v3,       \
                               uint32_t desc)                                  \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int##BITS##_t a = s390_vec_read_element##BITS(v2, i);            \
        const uint8_t shift = s390_vec_read_element##BITS(v3, i) & (BITS - 1); \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a >> shift);                       \
    }                                                                          \
}
DEF_VESRAV(8)
DEF_VESRAV(16)
DEF_VESRAV(32)
DEF_VESRAV(64)

#define DEF_VESRLV(BITS)                                                       \
void HELPER(gvec_vesrlv##BITS)(void *v1, const void *v2, const void *v3,       \
                               uint32_t desc)                                  \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint8_t shift = s390_vec_read_element##BITS(v3, i) & (BITS - 1); \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a >> shift);                       \
    }                                                                          \
}
DEF_VESRLV(8)
DEF_VESRLV(16)
DEF_VESRLV(32)
DEF_VESRLV(64)

#define DEF_VESL(BITS)                                                         \
void HELPER(gvec_vesl##BITS)(void *v1, const void *v3, uint64_t shift,         \
                             uint32_t desc)                                    \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a << shift);                       \
    }                                                                          \
}
DEF_VESL(8)
DEF_VESL(16)

#define DEF_VESRA(BITS)                                                        \
void HELPER(gvec_vesra##BITS)(void *v1, const void *v3, uint64_t shift,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const int##BITS##_t a = s390_vec_read_element##BITS(v3, i);            \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a >> shift);                       \
    }                                                                          \
}
DEF_VESRA(8)
DEF_VESRA(16)

#define DEF_VESRL(BITS)                                                        \
void HELPER(gvec_vesrl##BITS)(void *v1, const void *v3, uint64_t shift,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a >> shift);                       \
    }                                                                          \
}
DEF_VESRL(8)
DEF_VESRL(16)

void HELPER(gvec_vsl)(void *v1, const void *v2, uint64_t count,
                      uint32_t desc)
{
    s390_vec_shl(v1, v2, count);
}

void HELPER(gvec_vsldb)(void *v1, const void *v2, const void *v3,
                        uint32_t desc)
{
    const uint8_t src_idx = simd_data(desc);
    S390Vector t0;
    S390Vector t1;

    g_assert(src_idx > 0 && src_idx < 16);
    s390_vec_shl(&t0, v2, src_idx * 8);
    s390_vec_shr(&t1, v3, 128 - src_idx * 8);
    s390_vec_or(v1, &t0, &t1);
}

void HELPER(gvec_vsra)(void *v1, const void *v2, uint64_t count,
                       uint32_t desc)
{
    s390_vec_sar(v1, v2, count);
}

void HELPER(gvec_vsrl)(void *v1, const void *v2, uint64_t count,
                       uint32_t desc)
{
    s390_vec_shr(v1, v2, count);
}

#define DEF_VSCBI(BITS)                                                        \
void HELPER(gvec_vscbi##BITS)(void *v1, const void *v2, const void *v3,        \
                              uint32_t desc)                                   \
{                                                                              \
    int i;                                                                     \
                                                                               \
    for (i = 0; i < (128 / BITS); i++) {                                       \
        const uint##BITS##_t a = s390_vec_read_element##BITS(v2, i);           \
        const uint##BITS##_t b = s390_vec_read_element##BITS(v3, i);           \
                                                                               \
        s390_vec_write_element##BITS(v1, i, a < b);                            \
    }                                                                          \
}
DEF_VSCBI(8)
DEF_VSCBI(16)

void HELPER(gvec_vscbi128)(void *v1, const void *v2, const void *v3,
                           uint32_t desc)
{
    S390Vector *dst = v1;
    S390Vector tmp;

    dst->doubleword[0] = 0;
    dst->doubleword[1] = s390_vec_sub(&tmp, v2, v3);
}

void HELPER(gvec_vsbcbi128)(void *v1, const void *v2, const void *v3,
                            const void *v4, uint32_t desc)
{
    const S390Vector old_borrow = {
        .doubleword[0] = 0,
        .doubleword[1] = ((S390Vector *)v4)->doubleword[1] & 1,
    };
    S390Vector tmp, *dst = v1;
    bool borrow;

    borrow = s390_vec_sub(&tmp, v2, v3);
    borrow |= s390_vec_sub(&tmp, &tmp, &old_borrow);
    dst->doubleword[0] = 0;
    dst->doubleword[1] = borrow;
}

void HELPER(gvec_vtm)(void *v1, const void *v2, CPUS390XState *env,
                      uint32_t desc)
{
    S390Vector tmp;

    s390_vec_and(&tmp, v1, v2);
    if (s390_vec_is_zero(&tmp)) {
        /* Selected bits all zeros; or all mask bits zero */
        env->cc_op = 0;
    } else if (s390_vec_equal(&tmp, v2)) {
        /* Selected bits all ones */
        env->cc_op = 3;
    } else {
        /* Selected bits a mix of zeros and ones */
        env->cc_op = 1;
    }
}
