/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HELPER_OVERRIDES_H
#define HELPER_OVERRIDES_H

/*
 * Here is a primer to understand the tag names for load/store instructions
 *
 * Data types
 *      b        signed byte                       r0 = memb(r2+#0)
 *     ub        unsigned byte                     r0 = memub(r2+#0)
 *      h        signed half word (16 bits)        r0 = memh(r2+#0)
 *     uh        unsigned half word                r0 = memuh(r2+#0)
 *      i        integer (32 bits)                 r0 = memw(r2+#0)
 *      d        double word (64 bits)             r1:0 = memd(r2+#0)
 *
 * Addressing modes
 *     _io       indirect with offset              r0 = memw(r1+#4)
 *     _ur       absolute with register offset     r0 = memw(r1<<#4+##variable)
 *     _rr       indirect with register offset     r0 = memw(r1+r4<<#2)
 *     gp        global pointer relative           r0 = memw(gp+#200)
 *     _sp       stack pointer relative            r0 = memw(r29+#12)
 *     _ap       absolute set                      r0 = memw(r1=##variable)
 *     _pr       post increment register           r0 = memw(r1++m1)
 *     _pbr      post increment bit reverse        r0 = memw(r1++m1:brev)
 *     _pi       post increment immediate          r0 = memb(r1++#1)
 *     _pci      post increment circular immediate r0 = memw(r1++#4:circ(m0))
 *     _pcr      post increment circular register  r0 = memw(r1++I:circ(m0))
 */

/* Macros for complex addressing modes */
#define GET_EA_ap \
    do { \
        fEA_IMM(UiV); \
        tcg_gen_movi_tl(ReV, UiV); \
    } while (0)
#define GET_EA_pr \
    do { \
        fEA_REG(RxV); \
        fPM_M(RxV, MuV); \
    } while (0)
#define GET_EA_pbr \
    do { \
        fEA_BREVR(RxV); \
        fPM_M(RxV, MuV); \
    } while (0)
#define GET_EA_pi \
    do { \
        fEA_REG(RxV); \
        fPM_I(RxV, siV); \
    } while (0)
#define GET_EA_pci \
    do { \
        fEA_REG(RxV); \
        fPM_CIRI(RxV, siV, MuV); \
    } while (0)
#define GET_EA_pcr(SHIFT) \
    do { \
        fEA_REG(RxV); \
        fPM_CIRR(RxV, fREAD_IREG(MuV, (SHIFT)), MuV); \
    } while (0)

/*
 * Many instructions will work with just macro redefinitions
 * with the caveat that they need a tmp variable to carry a
 * value between them.
 */
#define fWRAP_tmp(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        SHORTCODE; \
        tcg_temp_free(tmp); \
    } while (0)

/* Byte load instructions */
#define fWRAP_L2_loadrub_io(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrb_io(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadrub_ur(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L4_loadrb_ur(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadrub_rr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L4_loadrb_rr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrubgp(GENHLPR, SHORTCODE)       fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadrbgp(GENHLPR, SHORTCODE)        fWRAP_tmp(SHORTCODE)
#define fWRAP_SL1_loadrub_io(GENHLPR, SHORTCODE)     SHORTCODE
#define fWRAP_SL2_loadrb_io(GENHLPR, SHORTCODE)      SHORTCODE

/* Half word load instruction */
#define fWRAP_L2_loadruh_io(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrh_io(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadruh_ur(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L4_loadrh_ur(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadruh_rr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L4_loadrh_rr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadruhgp(GENHLPR, SHORTCODE)       fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadrhgp(GENHLPR, SHORTCODE)        fWRAP_tmp(SHORTCODE)
#define fWRAP_SL2_loadruh_io(GENHLPR, SHORTCODE)     SHORTCODE
#define fWRAP_SL2_loadrh_io(GENHLPR, SHORTCODE)      SHORTCODE

/* Word load instructions */
#define fWRAP_L2_loadri_io(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadri_ur(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadri_rr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrigp(GENHLPR, SHORTCODE)        fWRAP_tmp(SHORTCODE)
#define fWRAP_SL1_loadri_io(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_SL2_loadri_sp(GENHLPR, SHORTCODE)      fWRAP_tmp(SHORTCODE)

/* Double word load instructions */
#define fWRAP_L2_loadrd_io(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadrd_ur(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L4_loadrd_rr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrdgp(GENHLPR, SHORTCODE)        fWRAP_tmp(SHORTCODE)
#define fWRAP_SL2_loadrd_sp(GENHLPR, SHORTCODE)      fWRAP_tmp(SHORTCODE)

/* Instructions with multiple definitions */
#define fWRAP_LOAD_AP(RES, SIZE, SIGN) \
    do { \
        fMUST_IMMEXT(UiV); \
        fEA_IMM(UiV); \
        fLOAD(1, SIZE, SIGN, EA, RES); \
        tcg_gen_movi_tl(ReV, UiV); \
    } while (0)

#define fWRAP_L4_loadrub_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RdV, 1, u)
#define fWRAP_L4_loadrb_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RdV, 1, s)
#define fWRAP_L4_loadruh_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RdV, 2, u)
#define fWRAP_L4_loadrh_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RdV, 2, s)
#define fWRAP_L4_loadri_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RdV, 4, u)
#define fWRAP_L4_loadrd_ap(GENHLPR, SHORTCODE) \
    fWRAP_LOAD_AP(RddV, 8, u)

#define fWRAP_L2_loadrub_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadrb_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadruh_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadrh_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadri_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)
#define fWRAP_L2_loadrd_pci(GENHLPR, SHORTCODE) \
      fWRAP_tmp(SHORTCODE)

#define fWRAP_PCR(SHIFT, LOAD) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        fEA_REG(RxV); \
        fREAD_IREG(MuV, SHIFT); \
        gen_fcircadd(RxV, ireg, MuV, fREAD_CSREG(MuN)); \
        LOAD; \
        tcg_temp_free(tmp); \
        tcg_temp_free(ireg); \
    } while (0)

#define fWRAP_L2_loadrub_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(0, fLOAD(1, 1, u, EA, RdV))
#define fWRAP_L2_loadrb_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(0, fLOAD(1, 1, s, EA, RdV))
#define fWRAP_L2_loadruh_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(1, fLOAD(1, 2, u, EA, RdV))
#define fWRAP_L2_loadrh_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(1, fLOAD(1, 2, s, EA, RdV))
#define fWRAP_L2_loadri_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(2, fLOAD(1, 4, u, EA, RdV))
#define fWRAP_L2_loadrd_pcr(GENHLPR, SHORTCODE) \
      fWRAP_PCR(3, fLOAD(1, 8, u, EA, RddV))

#define fWRAP_L2_loadrub_pr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrub_pbr(GENHLPR, SHORTCODE)     SHORTCODE
#define fWRAP_L2_loadrub_pi(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrb_pr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrb_pbr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrb_pi(GENHLPR, SHORTCODE)       SHORTCODE;
#define fWRAP_L2_loadruh_pr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadruh_pbr(GENHLPR, SHORTCODE)     SHORTCODE
#define fWRAP_L2_loadruh_pi(GENHLPR, SHORTCODE)      SHORTCODE;
#define fWRAP_L2_loadrh_pr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrh_pbr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrh_pi(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadri_pr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadri_pbr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadri_pi(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrd_pr(GENHLPR, SHORTCODE)       SHORTCODE
#define fWRAP_L2_loadrd_pbr(GENHLPR, SHORTCODE)      SHORTCODE
#define fWRAP_L2_loadrd_pi(GENHLPR, SHORTCODE)       SHORTCODE

/*
 * These instructions load 2 bytes and places them in
 * two halves of the destination register.
 * The GET_EA macro determines the addressing mode.
 * The fGB macro determines whether to zero-extend or
 * sign-extend.
 */
#define fWRAP_loadbXw2(GET_EA, fGB) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv tmpV = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        int i; \
        GET_EA; \
        fLOAD(1, 2, u, EA, tmpV); \
        tcg_gen_movi_tl(RdV, 0); \
        for (i = 0; i < 2; i++) { \
            fSETHALF(i, RdV, fGB(i, tmpV)); \
        } \
        tcg_temp_free(ireg); \
        tcg_temp_free(tmp); \
        tcg_temp_free(tmpV); \
        tcg_temp_free(BYTE); \
    } while (0)

#define fWRAP_L2_loadbzw2_io(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(fEA_RI(RsV, siV), fGETUBYTE)
#define fWRAP_L4_loadbzw2_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(fEA_IRs(UiV, RtV, uiV), fGETUBYTE)
#define fWRAP_L2_loadbsw2_io(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(fEA_RI(RsV, siV), fGETBYTE)
#define fWRAP_L4_loadbsw2_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(fEA_IRs(UiV, RtV, uiV), fGETBYTE)
#define fWRAP_L4_loadbzw2_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_ap, fGETUBYTE)
#define fWRAP_L2_loadbzw2_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pr, fGETUBYTE)
#define fWRAP_L2_loadbzw2_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pbr, fGETUBYTE)
#define fWRAP_L2_loadbzw2_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pi, fGETUBYTE)
#define fWRAP_L4_loadbsw2_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_ap, fGETBYTE)
#define fWRAP_L2_loadbsw2_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pr, fGETBYTE)
#define fWRAP_L2_loadbsw2_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pbr, fGETBYTE)
#define fWRAP_L2_loadbsw2_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pi, fGETBYTE)
#define fWRAP_L2_loadbzw2_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pci, fGETUBYTE)
#define fWRAP_L2_loadbsw2_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pci, fGETBYTE)
#define fWRAP_L2_loadbzw2_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pcr(1), fGETUBYTE)
#define fWRAP_L2_loadbsw2_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw2(GET_EA_pcr(1), fGETBYTE)

/*
 * These instructions load 4 bytes and places them in
 * four halves of the destination register pair.
 * The GET_EA macro determines the addressing mode.
 * The fGB macro determines whether to zero-extend or
 * sign-extend.
 */
#define fWRAP_loadbXw4(GET_EA, fGB) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv tmpV = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        int i; \
        GET_EA; \
        fLOAD(1, 4, u, EA, tmpV);  \
        tcg_gen_movi_i64(RddV, 0); \
        for (i = 0; i < 4; i++) { \
            fSETHALF(i, RddV, fGB(i, tmpV));  \
        }  \
        tcg_temp_free(ireg); \
        tcg_temp_free(tmp); \
        tcg_temp_free(tmpV); \
        tcg_temp_free(BYTE); \
    } while (0)

#define fWRAP_L2_loadbzw4_io(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(fEA_RI(RsV, siV), fGETUBYTE)
#define fWRAP_L4_loadbzw4_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(fEA_IRs(UiV, RtV, uiV), fGETUBYTE)
#define fWRAP_L2_loadbsw4_io(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(fEA_RI(RsV, siV), fGETBYTE)
#define fWRAP_L4_loadbsw4_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(fEA_IRs(UiV, RtV, uiV), fGETBYTE)
#define fWRAP_L2_loadbzw4_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pci, fGETUBYTE)
#define fWRAP_L2_loadbsw4_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pci, fGETBYTE)
#define fWRAP_L2_loadbzw4_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pcr(2), fGETUBYTE)
#define fWRAP_L2_loadbsw4_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pcr(2), fGETBYTE)
#define fWRAP_L4_loadbzw4_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_ap, fGETUBYTE)
#define fWRAP_L2_loadbzw4_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pr, fGETUBYTE)
#define fWRAP_L2_loadbzw4_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pbr, fGETUBYTE)
#define fWRAP_L2_loadbzw4_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pi, fGETUBYTE)
#define fWRAP_L4_loadbsw4_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_ap, fGETBYTE)
#define fWRAP_L2_loadbsw4_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pr, fGETBYTE)
#define fWRAP_L2_loadbsw4_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pbr, fGETBYTE)
#define fWRAP_L2_loadbsw4_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadbXw4(GET_EA_pi, fGETBYTE)

/*
 * These instructions load a half word, shift the destination right by 16 bits
 * and place the loaded value in the high half word of the destination pair.
 * The GET_EA macro determines the addressing mode.
 */
#define fWRAP_loadalignh(GET_EA) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv tmpV = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        READ_REG_PAIR(RyyV, RyyN); \
        GET_EA;  \
        fLOAD(1, 2, u, EA, tmpV);  \
        tcg_gen_extu_i32_i64(tmp_i64, tmpV); \
        tcg_gen_shli_i64(tmp_i64, tmp_i64, 48); \
        tcg_gen_shri_i64(RyyV, RyyV, 16); \
        tcg_gen_or_i64(RyyV, RyyV, tmp_i64); \
        tcg_temp_free(ireg); \
        tcg_temp_free(tmp); \
        tcg_temp_free(tmpV); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fWRAP_L4_loadalignh_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(fEA_IRs(UiV, RtV, uiV))
#define fWRAP_L2_loadalignh_io(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(fEA_RI(RsV, siV))
#define fWRAP_L2_loadalignh_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_pci)
#define fWRAP_L2_loadalignh_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_pcr(1))
#define fWRAP_L4_loadalignh_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_ap)
#define fWRAP_L2_loadalignh_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_pr)
#define fWRAP_L2_loadalignh_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_pbr)
#define fWRAP_L2_loadalignh_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadalignh(GET_EA_pi)

/* Same as above, but loads a byte instead of half word */
#define fWRAP_loadalignb(GET_EA) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv tmpV = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        READ_REG_PAIR(RyyV, RyyN); \
        GET_EA;  \
        fLOAD(1, 1, u, EA, tmpV);  \
        tcg_gen_extu_i32_i64(tmp_i64, tmpV); \
        tcg_gen_shli_i64(tmp_i64, tmp_i64, 56); \
        tcg_gen_shri_i64(RyyV, RyyV, 8); \
        tcg_gen_or_i64(RyyV, RyyV, tmp_i64); \
        tcg_temp_free(ireg); \
        tcg_temp_free(tmp); \
        tcg_temp_free(tmpV); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fWRAP_L2_loadalignb_io(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(fEA_RI(RsV, siV))
#define fWRAP_L4_loadalignb_ur(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(fEA_IRs(UiV, RtV, uiV))
#define fWRAP_L2_loadalignb_pci(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_pci)
#define fWRAP_L2_loadalignb_pcr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_pcr(0))
#define fWRAP_L4_loadalignb_ap(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_ap)
#define fWRAP_L2_loadalignb_pr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_pr)
#define fWRAP_L2_loadalignb_pbr(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_pbr)
#define fWRAP_L2_loadalignb_pi(GENHLPR, SHORTCODE) \
    fWRAP_loadalignb(GET_EA_pi)

/*
 * Predicated loads
 * Here is a primer to understand the tag names
 *
 * Predicate used
 *      t        true "old" value                  if (p0) r0 = memb(r2+#0)
 *      f        false "old" value                 if (!p0) r0 = memb(r2+#0)
 *      tnew     true "new" value                  if (p0.new) r0 = memb(r2+#0)
 *      fnew     false "new" value                 if (!p0.new) r0 = memb(r2+#0)
 */
#define fWRAP_PRED_LOAD(GET_EA, PRED, SIZE, SIGN) \
    do { \
        TCGv LSB = tcg_temp_local_new(); \
        TCGLabel *label = gen_new_label(); \
        GET_EA; \
        PRED;  \
        PRED_LOAD_CANCEL(LSB, EA); \
        tcg_gen_movi_tl(RdV, 0); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
            fLOAD(1, SIZE, SIGN, EA, RdV); \
        gen_set_label(label); \
        tcg_temp_free(LSB); \
    } while (0)

#define fWRAP_L2_ploadrubt_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 1, u)
#define fWRAP_L2_ploadrubt_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 1, u)
#define fWRAP_L2_ploadrubf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 1, u)
#define fWRAP_L2_ploadrubf_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 1, u)
#define fWRAP_L2_ploadrubtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 1, u)
#define fWRAP_L2_ploadrubfnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 1, u)
#define fWRAP_L4_ploadrubt_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 1, u)
#define fWRAP_L4_ploadrubf_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 1, u)
#define fWRAP_L4_ploadrubtnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 1, u)
#define fWRAP_L4_ploadrubfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 1, u)
#define fWRAP_L2_ploadrubtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 1, u)
#define fWRAP_L2_ploadrubfnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 1, u)
#define fWRAP_L4_ploadrubt_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 1, u)
#define fWRAP_L4_ploadrubf_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 1, u)
#define fWRAP_L4_ploadrubtnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 1, u)
#define fWRAP_L4_ploadrubfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 1, u)
#define fWRAP_L2_ploadrbt_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 1, s)
#define fWRAP_L2_ploadrbt_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 1, s)
#define fWRAP_L2_ploadrbf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 1, s)
#define fWRAP_L2_ploadrbf_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 1, s)
#define fWRAP_L2_ploadrbtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 1, s)
#define fWRAP_L2_ploadrbfnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 1, s)
#define fWRAP_L4_ploadrbt_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 1, s)
#define fWRAP_L4_ploadrbf_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 1, s)
#define fWRAP_L4_ploadrbtnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 1, s)
#define fWRAP_L4_ploadrbfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 1, s)
#define fWRAP_L2_ploadrbtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 1, s)
#define fWRAP_L2_ploadrbfnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD({ fEA_REG(RxV); fPM_I(RxV, siV); }, fLSBNEWNOT(PtN), 1, s)
#define fWRAP_L4_ploadrbt_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 1, s)
#define fWRAP_L4_ploadrbf_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 1, s)
#define fWRAP_L4_ploadrbtnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 1, s)
#define fWRAP_L4_ploadrbfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 1, s)

#define fWRAP_L2_ploadruht_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 2, u)
#define fWRAP_L2_ploadruht_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 2, u)
#define fWRAP_L2_ploadruhf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 2, u)
#define fWRAP_L2_ploadruhf_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 2, u)
#define fWRAP_L2_ploadruhtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 2, u)
#define fWRAP_L2_ploadruhfnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 2, u)
#define fWRAP_L4_ploadruht_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 2, u)
#define fWRAP_L4_ploadruhf_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 2, u)
#define fWRAP_L4_ploadruhtnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 2, u)
#define fWRAP_L4_ploadruhfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 2, u)
#define fWRAP_L2_ploadruhtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 2, u)
#define fWRAP_L2_ploadruhfnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 2, u)
#define fWRAP_L4_ploadruht_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 2, u)
#define fWRAP_L4_ploadruhf_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 2, u)
#define fWRAP_L4_ploadruhtnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 2, u)
#define fWRAP_L4_ploadruhfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 2, u)
#define fWRAP_L2_ploadrht_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 2, s)
#define fWRAP_L2_ploadrht_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 2, s)
#define fWRAP_L2_ploadrhf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 2, s)
#define fWRAP_L2_ploadrhf_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 2, s)
#define fWRAP_L2_ploadrhtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 2, s)
#define fWRAP_L2_ploadrhfnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 2, s)
#define fWRAP_L4_ploadrht_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 2, s)
#define fWRAP_L4_ploadrhf_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 2, s)
#define fWRAP_L4_ploadrhtnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 2, s)
#define fWRAP_L4_ploadrhfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 2, s)
#define fWRAP_L2_ploadrhtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 2, s)
#define fWRAP_L2_ploadrhfnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 2, s)
#define fWRAP_L4_ploadrht_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 2, s)
#define fWRAP_L4_ploadrhf_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 2, s)
#define fWRAP_L4_ploadrhtnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 2, s)
#define fWRAP_L4_ploadrhfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 2, s)

#define fWRAP_L2_ploadrit_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 4, u)
#define fWRAP_L2_ploadrit_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 4, u)
#define fWRAP_L2_ploadrif_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 4, u)
#define fWRAP_L2_ploadrif_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 4, u)
#define fWRAP_L2_ploadritnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 4, u)
#define fWRAP_L2_ploadrifnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 4, u)
#define fWRAP_L4_ploadrit_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 4, u)
#define fWRAP_L4_ploadrif_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 4, u)
#define fWRAP_L4_ploadritnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 4, u)
#define fWRAP_L4_ploadrifnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 4, u)
#define fWRAP_L2_ploadritnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 4, u)
#define fWRAP_L2_ploadrifnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 4, u)
#define fWRAP_L4_ploadrit_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 4, u)
#define fWRAP_L4_ploadrif_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 4, u)
#define fWRAP_L4_ploadritnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 4, u)
#define fWRAP_L4_ploadrifnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 4, u)

/* Predicated loads into a register pair */
#define fWRAP_PRED_LOAD_PAIR(GET_EA, PRED) \
    do { \
        TCGv LSB = tcg_temp_local_new(); \
        TCGLabel *label = gen_new_label(); \
        GET_EA; \
        PRED;  \
        PRED_LOAD_CANCEL(LSB, EA); \
        tcg_gen_movi_i64(RddV, 0); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
            fLOAD(1, 8, u, EA, RddV); \
        gen_set_label(label); \
        tcg_temp_free(LSB); \
    } while (0)

#define fWRAP_L2_ploadrdt_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBOLD(PtV))
#define fWRAP_L2_ploadrdt_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(GET_EA_pi, fLSBOLD(PtV))
#define fWRAP_L2_ploadrdf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV))
#define fWRAP_L2_ploadrdf_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(GET_EA_pi, fLSBOLDNOT(PtV))
#define fWRAP_L2_ploadrdtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBNEW(PtN))
#define fWRAP_L2_ploadrdfnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN))
#define fWRAP_L4_ploadrdt_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV))
#define fWRAP_L4_ploadrdf_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV))
#define fWRAP_L4_ploadrdtnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN))
#define fWRAP_L4_ploadrdfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN))
#define fWRAP_L2_ploadrdtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(GET_EA_pi, fLSBNEW(PtN))
#define fWRAP_L2_ploadrdfnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(GET_EA_pi, fLSBNEWNOT(PtN))
#define fWRAP_L4_ploadrdt_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBOLD(PtV))
#define fWRAP_L4_ploadrdf_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBOLDNOT(PtV))
#define fWRAP_L4_ploadrdtnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBNEW(PtN))
#define fWRAP_L4_ploadrdfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBNEWNOT(PtN))

/* load-locked and store-locked */
#define fWRAP_L2_loadw_locked(GENHLPR, SHORTCODE) \
    SHORTCODE
#define fWRAP_L4_loadd_locked(GENHLPR, SHORTCODE) \
    SHORTCODE
#define fWRAP_S2_storew_locked(GENHLPR, SHORTCODE) \
    do { SHORTCODE; READ_PREG(PdV, PdN); } while (0)
#define fWRAP_S4_stored_locked(GENHLPR, SHORTCODE) \
    do { SHORTCODE; READ_PREG(PdV, PdN); } while (0)

#define fWRAP_STORE(SHORTCODE) \
    do { \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        TCGv NEWREG_ST = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        SHORTCODE; \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
        tcg_temp_free(NEWREG_ST); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_STORE_ap(STORE) \
    do { \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        TCGv NEWREG_ST = tcg_temp_new(); \
        { \
            fEA_IMM(UiV); \
            STORE; \
            tcg_gen_movi_tl(ReV, UiV); \
        } \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
        tcg_temp_free(NEWREG_ST); \
    } while (0)

#define fWRAP_STORE_pcr(SHIFT, STORE) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        TCGv NEWREG_ST = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        fEA_REG(RxV); \
        fPM_CIRR(RxV, fREAD_IREG(MuV, SHIFT), MuV); \
        STORE; \
        tcg_temp_free(ireg); \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
        tcg_temp_free(NEWREG_ST); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_S2_storerb_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerb_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerb_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 1, EA, fGETBYTE(0, RtV)))
#define fWRAP_S2_storerb_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerb_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerb_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerb_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerb_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(0, fSTORE(1, 1, EA, fGETBYTE(0, RtV)))
#define fWRAP_S4_storerb_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerbnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storeirb_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerbgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS1_storeb_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS2_storebi0(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerh_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerh_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerh_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 2, EA, fGETHALF(0, RtV)))
#define fWRAP_S2_storerh_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerh_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerh_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerh_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerh_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(0, RtV)))
#define fWRAP_S4_storerh_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storeirh_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerhgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS2_storeh_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerf_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerf_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerf_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 2, EA, fGETHALF(1, RtV)))
#define fWRAP_S2_storerf_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerf_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerf_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerf_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerf_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(1, RtV)))
#define fWRAP_S4_storerf_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerfgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storeri_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storeri_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storeri_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 4, EA, RtV))
#define fWRAP_S2_storeri_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storeri_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storeri_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storeri_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storeri_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(2, fSTORE(1, 4, EA, RtV))
#define fWRAP_S4_storeri_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerinew_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storeiri_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerigp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS1_storew_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS2_storew_sp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS2_storewi0(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerd_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerd_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerd_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 8, EA, RttV))
#define fWRAP_S2_storerd_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerd_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerd_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerd_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerd_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(3, fSTORE(1, 8, EA, RttV))
#define fWRAP_S4_storerd_rr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerdgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_SS2_stored_sp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerbnew_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerbnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerbnew_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 1, EA, fGETBYTE(0, fNEWREG_ST(NtN))))
#define fWRAP_S2_storerbnew_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerbnew_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerbnew_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerbnew_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerbnew_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(0, fSTORE(1, 1, EA, fGETBYTE(0, fNEWREG_ST(NtN))))
#define fWRAP_S2_storerbnewgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerhnew_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerhnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerhnew_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 2, EA, fGETHALF(0, fNEWREG_ST(NtN))))
#define fWRAP_S2_storerhnew_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerhnew_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerhnew_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerhnew_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerhnew_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(0, fNEWREG_ST(NtN))))
#define fWRAP_S2_storerhnewgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

#define fWRAP_S2_storerinew_io(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerinew_pi(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerinew_ap(GENHLPR, SHORTCODE) \
    fWRAP_STORE_ap(fSTORE(1, 4, EA, fNEWREG_ST(NtN)))
#define fWRAP_S2_storerinew_pr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S4_storerinew_ur(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerinew_pbr(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerinew_pci(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)
#define fWRAP_S2_storerinew_pcr(GENHLPR, SHORTCODE) \
    fWRAP_STORE_pcr(2, fSTORE(1, 4, EA, fNEWREG_ST(NtN)))
#define fWRAP_S2_storerinewgp(GENHLPR, SHORTCODE) \
    fWRAP_STORE(SHORTCODE)

/* Predicated stores */
#define fWRAP_PRED_STORE(GET_EA, PRED, SRC, SIZE, INC) \
    do { \
        TCGv LSB = tcg_temp_local_new(); \
        TCGv NEWREG_ST = tcg_temp_local_new(); \
        TCGv BYTE = tcg_temp_local_new(); \
        TCGv HALF = tcg_temp_local_new(); \
        TCGLabel *label = gen_new_label(); \
        GET_EA; \
        PRED;  \
        PRED_STORE_CANCEL(LSB, EA); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
            INC; \
            fSTORE(1, SIZE, EA, SRC); \
        gen_set_label(label); \
        tcg_temp_free(LSB); \
        tcg_temp_free(NEWREG_ST); \
        tcg_temp_free(BYTE); \
        tcg_temp_free(HALF); \
    } while (0)

#define NOINC    do {} while (0)

#define fWRAP_S4_pstorerinewfnew_rr(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_RRs(RsV, RuV, uiV), fLSBNEWNOT(PvN), \
                     hex_new_value[NtX], 4, NOINC)
#define fWRAP_S2_pstorerdtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_REG(RxV), fLSBNEW(PvN), \
                     RttV, 8, tcg_gen_addi_tl(RxV, RxV, IMMNO(0)))
#define fWRAP_S4_pstorerdtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEW(PvN), \
                     RttV, 8, NOINC)
#define fWRAP_S4_pstorerbtnew_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEW(PvN), \
                     fGETBYTE(0, RtV), 1, NOINC)
#define fWRAP_S2_pstorerhtnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_REG(RxV), fLSBNEW(PvN), \
                     fGETHALF(0, RtV), 2, tcg_gen_addi_tl(RxV, RxV, IMMNO(0)))
#define fWRAP_S2_pstoreritnew_pi(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_REG(RxV), fLSBNEW(PvN), \
                     RtV, 4, tcg_gen_addi_tl(RxV, RxV, IMMNO(0)))
#define fWRAP_S2_pstorerif_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLDNOT(PvV), \
                     RtV, 4, NOINC)
#define fWRAP_S4_pstorerit_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_IMM(uiV), fLSBOLD(PvV), \
                     RtV, 4, NOINC)
#define fWRAP_S2_pstorerinewf_io(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLDNOT(PvV), \
                     hex_new_value[NtX], 4, NOINC)
#define fWRAP_S4_pstorerbnewfnew_abs(GENHLPR, SHORTCODE) \
    fWRAP_PRED_STORE(fEA_IMM(uiV), fLSBNEWNOT(PvN), \
                     fGETBYTE(0, hex_new_value[NtX]), 1, NOINC)

/* We have to brute force memops because they have C math in the semantics */
#define fWRAP_MEMOP(GENHLPR, SHORTCODE, SIZE, OP) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        fEA_RI(RsV, uiV); \
        fLOAD(1, SIZE, s, EA, tmp); \
        OP; \
        fSTORE(1, SIZE, EA, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_L4_add_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_add_tl(tmp, tmp, RtV))
#define fWRAP_L4_add_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_add_tl(tmp, tmp, RtV))
#define fWRAP_L4_add_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_add_tl(tmp, tmp, RtV))
#define fWRAP_L4_sub_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fWRAP_L4_sub_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fWRAP_L4_sub_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fWRAP_L4_and_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_and_tl(tmp, tmp, RtV))
#define fWRAP_L4_and_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_and_tl(tmp, tmp, RtV))
#define fWRAP_L4_and_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_and_tl(tmp, tmp, RtV))
#define fWRAP_L4_or_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_or_tl(tmp, tmp, RtV))
#define fWRAP_L4_or_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_or_tl(tmp, tmp, RtV))
#define fWRAP_L4_or_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_or_tl(tmp, tmp, RtV))
#define fWRAP_L4_iadd_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fWRAP_L4_iadd_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fWRAP_L4_iadd_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fWRAP_L4_isub_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fWRAP_L4_isub_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fWRAP_L4_isub_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fWRAP_L4_iand_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fWRAP_L4_iand_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fWRAP_L4_iand_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fWRAP_L4_ior_memopw_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 4, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))
#define fWRAP_L4_ior_memopb_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 1, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))
#define fWRAP_L4_ior_memoph_io(GENHLPR, SHORTCODE) \
    fWRAP_MEMOP(GENHLPR, SHORTCODE, 2, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))

/* dczeroa clears the 32 byte cache line at the address given */
#define fWRAP_Y2_dczeroa(GENHLPR, SHORTCODE) SHORTCODE

/* We have to brute force allocframe because it has C math in the semantics */
#define fWRAP_S2_allocframe(GENHLPR, SHORTCODE) \
    do { \
        TCGv_i64 scramble_tmp = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        { fEA_RI(RxV, -8); \
          fSTORE(1, 8, EA, fFRAME_SCRAMBLE((fCAST8_8u(fREAD_LR()) << 32) | \
                                           fCAST4_4u(fREAD_FP()))); \
          fWRITE_FP(EA); \
          fFRAMECHECK(EA - uiV, EA); \
          tcg_gen_subi_tl(RxV, EA, uiV); \
        } \
        tcg_temp_free_i64(scramble_tmp); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_SS2_allocframe(GENHLPR, SHORTCODE) \
    do { \
        TCGv_i64 scramble_tmp = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        { fEA_RI(fREAD_SP(), -8); \
          fSTORE(1, 8, EA, fFRAME_SCRAMBLE((fCAST8_8u(fREAD_LR()) << 32) | \
                                           fCAST4_4u(fREAD_FP()))); \
          fWRITE_FP(EA); \
          fFRAMECHECK(EA - uiV, EA); \
          tcg_gen_subi_tl(tmp, EA, uiV); \
          fWRITE_SP(tmp); \
        } \
        tcg_temp_free_i64(scramble_tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* Also have to brute force the deallocframe variants */
#define fWRAP_L2_deallocframe(GENHLPR, SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        { \
          fEA_REG(RsV); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          tcg_gen_mov_i64(RddV, fFRAME_UNSCRAMBLE(tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          fWRITE_SP(tmp); \
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fWRAP_SL2_deallocframe(GENHLPR, SHORTCODE) \
    do { \
        TCGv WORD = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        { \
          fEA_REG(fREAD_FP()); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          fFRAME_UNSCRAMBLE(tmp_i64); \
          fWRITE_LR(fGETWORD(1, tmp_i64)); \
          fWRITE_FP(fGETWORD(0, tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          fWRITE_SP(tmp); \
        } \
        tcg_temp_free(WORD); \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fWRAP_L4_return(GENHLPR, SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        { \
          fEA_REG(RsV); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          tcg_gen_mov_i64(RddV, fFRAME_UNSCRAMBLE(tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          fWRITE_SP(tmp); \
          fJUMPR(REG_LR, fGETWORD(1, RddV), COF_TYPE_JUMPR);\
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(WORD); \
    } while (0)

#define fWRAP_SL2_return(GENHLPR, SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        { \
          fEA_REG(fREAD_FP()); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          fFRAME_UNSCRAMBLE(tmp_i64); \
          fWRITE_LR(fGETWORD(1, tmp_i64)); \
          fWRITE_FP(fGETWORD(0, tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          fWRITE_SP(tmp); \
          fJUMPR(REG_LR, fGETWORD(1, tmp_i64), COF_TYPE_JUMPR);\
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(WORD); \
    } while (0)

/*
 * Conditional returns follow the same predicate naming convention as
 * predicated loads above
 */
#define fWRAP_COND_RETURN(PRED) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 LSB_i64 = tcg_temp_new_i64(); \
        TCGv zero = tcg_const_tl(0); \
        TCGv_i64 zero_i64 = tcg_const_i64(0); \
        TCGv_i64 unscramble = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        TCGv SP = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        fEA_REG(RsV); \
        PRED; \
        tcg_gen_extu_i32_i64(LSB_i64, LSB); \
        fLOAD(1, 8, u, EA, tmp_i64); \
        tcg_gen_mov_i64(unscramble, fFRAME_UNSCRAMBLE(tmp_i64)); \
        READ_REG_PAIR(RddV, HEX_REG_FP); \
        tcg_gen_movcond_i64(TCG_COND_NE, RddV, LSB_i64, zero_i64, \
                            unscramble, RddV); \
        tcg_gen_mov_tl(SP, hex_gpr[HEX_REG_SP]); \
        tcg_gen_addi_tl(tmp, EA, 8); \
        tcg_gen_movcond_tl(TCG_COND_NE, SP, LSB, zero, tmp, SP); \
        fWRITE_SP(SP); \
        gen_cond_return(LSB, fGETWORD(1, RddV)); \
        tcg_temp_free(LSB); \
        tcg_temp_free_i64(LSB_i64); \
        tcg_temp_free(zero); \
        tcg_temp_free_i64(zero_i64); \
        tcg_temp_free_i64(unscramble); \
        tcg_temp_free(WORD); \
        tcg_temp_free(SP); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_L4_return_t(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBOLD(PvV))
#define fWRAP_L4_return_f(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBOLDNOT(PvV))
#define fWRAP_L4_return_tnew_pt(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBNEW(PvN))
#define fWRAP_L4_return_fnew_pt(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBNEWNOT(PvN))
#define fWRAP_L4_return_tnew_pnt(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBNEW(PvN))
#define fWRAP_L4_return_tnew_pnt(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBNEW(PvN))
#define fWRAP_L4_return_fnew_pnt(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN(fLSBNEWNOT(PvN))

#define fWRAP_COND_RETURN_SUBINSN(PRED) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 LSB_i64 = tcg_temp_new_i64(); \
        TCGv zero = tcg_const_tl(0); \
        TCGv_i64 zero_i64 = tcg_const_i64(0); \
        TCGv_i64 unscramble = tcg_temp_new_i64(); \
        TCGv_i64 RddV = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        TCGv SP = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        fEA_REG(fREAD_FP()); \
        PRED; \
        tcg_gen_extu_i32_i64(LSB_i64, LSB); \
        fLOAD(1, 8, u, EA, tmp_i64); \
        tcg_gen_mov_i64(unscramble, fFRAME_UNSCRAMBLE(tmp_i64)); \
        READ_REG_PAIR(RddV, HEX_REG_FP); \
        tcg_gen_movcond_i64(TCG_COND_NE, RddV, LSB_i64, zero_i64, \
                            unscramble, RddV); \
        tcg_gen_mov_tl(SP, hex_gpr[HEX_REG_SP]); \
        tcg_gen_addi_tl(tmp, EA, 8); \
        tcg_gen_movcond_tl(TCG_COND_NE, SP, LSB, zero, tmp, SP); \
        fWRITE_SP(SP); \
        WRITE_REG_PAIR(HEX_REG_FP, RddV); \
        gen_cond_return(LSB, fGETWORD(1, RddV)); \
        tcg_temp_free(LSB); \
        tcg_temp_free_i64(LSB_i64); \
        tcg_temp_free(zero); \
        tcg_temp_free_i64(zero_i64); \
        tcg_temp_free_i64(unscramble); \
        tcg_temp_free_i64(RddV); \
        tcg_temp_free(WORD); \
        tcg_temp_free(SP); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_SL2_return_t(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN_SUBINSN(fLSBOLD(fREAD_P0()))
#define fWRAP_SL2_return_f(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN_SUBINSN(fLSBOLDNOT(fREAD_P0()))
#define fWRAP_SL2_return_tnew(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN_SUBINSN(fLSBNEW0)
#define fWRAP_SL2_return_fnew(GENHLPR, SHORTCODE) \
    fWRAP_COND_RETURN_SUBINSN(fLSBNEW0NOT)

/*
 * Mathematical operations with more than one definition require
 * special handling
 */
/*
 * Approximate reciprocal
 * r3,p1 = sfrecipa(r0, r1)
 */
#define fWRAP_F2_sfrecipa(GENHLPR, SHORTCODE) \
    do { \
        gen_helper_sfrecipa_val(RdV, cpu_env, RsV, RtV);  \
        gen_helper_sfrecipa_pred(PeV, cpu_env, RsV, RtV);  \
    } while (0)

/*
 * Approximation of the reciprocal square root
 * r1,p0 = sfinvsqrta(r0)
 */
#define fWRAP_F2_sfinvsqrta(GENHLPR, SHORTCODE) \
    do { \
        gen_helper_sfinvsqrta_val(RdV, cpu_env, RsV); \
        gen_helper_sfinvsqrta_pred(PeV, cpu_env, RsV); \
    } while (0)

#define fWRAP_A5_ACS(GENHLPR, SHORTCODE) \
    do { \
        gen_helper_vacsh_val(RxxV, cpu_env, RxxV, RssV, RttV); \
        gen_helper_vacsh_pred(PeV, cpu_env, RxxV, RssV, RttV); \
    } while (0)

/*
 * The following fWRAP macros are to speed up qemu
 * We can add more over time
 */

/*
 * Add or subtract with carry.
 * Predicate register is used as an extra input and output.
 * r5:4 = add(r1:0, r3:2, p1):carry
 */
#define fWRAP_A4_addp_c(GENHLPR, SHORTCODE) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 LSB_i64 = tcg_temp_new_i64(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_add_i64(RddV, RssV, RttV); \
        fLSBOLD(PxV); \
        tcg_gen_extu_i32_i64(LSB_i64, LSB); \
        tcg_gen_add_i64(RddV, RddV, LSB_i64); \
        fCARRY_FROM_ADD(RssV, RttV, LSB_i64); \
        tcg_gen_extrl_i64_i32(tmp, RssV); \
        f8BITSOF(PxV, tmp); \
        tcg_temp_free(LSB); \
        tcg_temp_free_i64(LSB_i64); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
    } while (0)

/* r5:4 = sub(r1:0, r3:2, p1):carry */
#define fWRAP_A4_subp_c(GENHLPR, SHORTCODE) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 LSB_i64 = tcg_temp_new_i64(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_not_i64(tmp_i64, RttV); \
        tcg_gen_add_i64(RddV, RssV, tmp_i64); \
        fLSBOLD(PxV); \
        tcg_gen_extu_i32_i64(LSB_i64, LSB); \
        tcg_gen_add_i64(RddV, RddV, LSB_i64); \
        fCARRY_FROM_ADD(RssV, tmp_i64, LSB_i64); \
        tcg_gen_extrl_i64_i32(tmp, RssV); \
        f8BITSOF(PxV, tmp); \
        tcg_temp_free(LSB); \
        tcg_temp_free_i64(LSB_i64); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
    } while (0)

/*
 * Compare each of the 8 unsigned bytes
 * The minimum is places in each byte of the destination.
 * Each bit of the predicate is set true if the bit from the first operand
 * is greater than the bit from the second operand.
 * r5:4,p1 = vminub(r1:0, r3:2)
 */
#define fWRAP_A6_vminub_RdP(GENHLPR, SHORTCODE) \
    do { \
        TCGv BYTE = tcg_temp_new(); \
        TCGv left = tcg_temp_new(); \
        TCGv right = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        int i; \
        tcg_gen_movi_tl(PeV, 0); \
        tcg_gen_movi_i64(RddV, 0); \
        for (i = 0; i < 8; i++) { \
            fGETUBYTE(i, RttV); \
            tcg_gen_mov_tl(left, BYTE); \
            fGETUBYTE(i, RssV); \
            tcg_gen_mov_tl(right, BYTE); \
            tcg_gen_setcond_tl(TCG_COND_GT, tmp, left, right); \
            fSETBIT(i, PeV, tmp); \
            fMIN(tmp, left, right); \
            fSETBYTE(i, RddV, tmp); \
        } \
        tcg_temp_free(BYTE); \
        tcg_temp_free(left); \
        tcg_temp_free(right); \
        tcg_temp_free(tmp); \
    } while (0)

#define fWRAP_J2_call(GENHLPR, SHORTCODE) \
    gen_call(riV)
#define fWRAP_J2_callr(GENHLPR, SHORTCODE) \
    gen_callr(RsV)

#define fWRAP_J2_loop0r(GENHLPR, SHORTCODE) \
    gen_loop0r(RsV, riV, insn)
#define fWRAP_J2_loop1r(GENHLPR, SHORTCODE) \
    gen_loop1r(RsV, riV, insn)

#define fWRAP_J2_endloop0(GENHLPR, SHORTCODE) \
    gen_endloop0()
#define fWRAP_J2_endloop1(GENHLPR, SHORTCODE) \
    gen_endloop1()

/*
 * Compound compare and jump instructions
 * Here is a primer to understand the tag names
 *
 * Comparison
 *      cmpeqi   compare equal to an immediate
 *      cmpgti   compare greater than an immediate
 *      cmpgtiu  compare greater than an unsigned immediate
 *      cmpeqn1  compare equal to negative 1
 *      cmpgtn1  compare greater than negative 1
 *      cmpeq    compare equal (two registers)
 *
 * Condition
 *      tp0      p0 is true     p0 = cmp.eq(r0,#5); if (p0.new) jump:nt address
 *      fp0      p0 is false    p0 = cmp.eq(r0,#5); if (!p0.new) jump:nt address
 *      tp1      p1 is true     p1 = cmp.eq(r0,#5); if (p1.new) jump:nt address
 *      fp1      p1 is false    p1 = cmp.eq(r0,#5); if (!p1.new) jump:nt address
 *
 * Prediction (not modelled in qemu)
 *      _nt      not taken
 *      _t       taken
 */
#define fWRAP_J4_cmpeqi_tp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_EQ, true, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_fp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_EQ, false, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_EQ, true, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_fp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_EQ, false, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_tp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_EQ, true, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_fp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_EQ, false, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_tp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_EQ, true, RsV, UiV, riV)
#define fWRAP_J4_cmpeqi_fp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_EQ, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_tp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GT, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_fp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GT, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GT, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_fp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GT, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_tp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GT, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_fp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GT, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_tp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GT, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgti_fp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GT, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_tp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GTU, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_fp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GTU, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GTU, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_fp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(0, TCG_COND_GTU, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_tp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GTU, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_fp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GTU, false, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_tp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GTU, true, RsV, UiV, riV)
#define fWRAP_J4_cmpgtui_fp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmpi_jmp(1, TCG_COND_GTU, false, RsV, UiV, riV)
#define fWRAP_J4_cmpeqn1_tp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_EQ, true, RsV, riV)
#define fWRAP_J4_cmpeqn1_fp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_EQ, false, RsV, riV)
#define fWRAP_J4_cmpeqn1_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_EQ, true, RsV, riV)
#define fWRAP_J4_cmpeqn1_fp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_EQ, false, RsV, riV)
#define fWRAP_J4_cmpeqn1_tp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_EQ, true, RsV, riV)
#define fWRAP_J4_cmpeqn1_fp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_EQ, false, RsV, riV)
#define fWRAP_J4_cmpeqn1_tp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_EQ, true, RsV, riV)
#define fWRAP_J4_cmpeqn1_fp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_EQ, false, RsV, riV)
#define fWRAP_J4_cmpgtn1_tp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_GT, true, RsV, riV)
#define fWRAP_J4_cmpgtn1_fp0_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_GT, false, RsV, riV)
#define fWRAP_J4_cmpgtn1_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_GT, true, RsV, riV)
#define fWRAP_J4_cmpgtn1_fp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(0, TCG_COND_GT, false, RsV, riV)
#define fWRAP_J4_cmpgtn1_tp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_GT, true, RsV, riV)
#define fWRAP_J4_cmpgtn1_fp1_jump_nt(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_GT, false, RsV, riV)
#define fWRAP_J4_cmpgtn1_tp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_GT, true, RsV, riV)
#define fWRAP_J4_cmpgtn1_fp1_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(1, TCG_COND_GT, false, RsV, riV)
#define fWRAP_J4_cmpeq_tp0_jump_t(GENHLPR, SHORTCODE) \
    gen_cmpnd_cmp_jmp(0, TCG_COND_EQ, true, RsV, RtV, riV)

#endif
