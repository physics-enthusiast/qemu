/* Exceptions */
DEF_HELPER_2(raise_exception, noreturn, env, i32)

/* Floating Point - rounding mode */
DEF_HELPER_FLAGS_2(set_rounding_mode, TCG_CALL_NO_WG, void, env, i32)

/* Floating Point - fused */
DEF_HELPER_FLAGS_4(fmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)

/* Floating Point - Single Precision */
DEF_HELPER_FLAGS_3(fadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmul_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fdiv_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmin_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmax_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(fsqrt_s, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_3(fle_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(flt_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(feq_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_w_s, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_wu_s, TCG_CALL_NO_RWG, tl, env, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_l_s, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_lu_s, TCG_CALL_NO_RWG, tl, env, i64)
#endif
DEF_HELPER_FLAGS_2(fcvt_s_w, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_s_wu, TCG_CALL_NO_RWG, i64, env, tl)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_s_l, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_s_lu, TCG_CALL_NO_RWG, i64, env, tl)
#endif
DEF_HELPER_FLAGS_1(fclass_s, TCG_CALL_NO_RWG_SE, tl, i64)

/* Floating Point - Double Precision */
DEF_HELPER_FLAGS_3(fadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmul_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fdiv_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmin_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmax_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_s_d, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(fcvt_d_s, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(fsqrt_d, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_3(fle_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(flt_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(feq_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_w_d, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_wu_d, TCG_CALL_NO_RWG, tl, env, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_l_d, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_lu_d, TCG_CALL_NO_RWG, tl, env, i64)
#endif
DEF_HELPER_FLAGS_2(fcvt_d_w, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_d_wu, TCG_CALL_NO_RWG, i64, env, tl)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_d_l, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_d_lu, TCG_CALL_NO_RWG, i64, env, tl)
#endif
DEF_HELPER_FLAGS_1(fclass_d, TCG_CALL_NO_RWG_SE, tl, i64)

/* Special functions */
DEF_HELPER_3(csrrw, tl, env, tl, tl)
DEF_HELPER_4(csrrs, tl, env, tl, tl, tl)
DEF_HELPER_4(csrrc, tl, env, tl, tl, tl)
#ifndef CONFIG_USER_ONLY
DEF_HELPER_2(sret, tl, env, tl)
DEF_HELPER_2(mret, tl, env, tl)
DEF_HELPER_1(wfi, void, env)
DEF_HELPER_1(tlb_flush, void, env)
#endif
/* Vector functions */
DEF_HELPER_3(vsetvl, tl, env, tl, tl)
DEF_HELPER_5(vlb_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_b_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlb_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlh_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlw_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlw_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlw_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlw_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_b_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vle_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_b_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbu_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhu_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwu_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwu_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwu_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwu_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_b_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsb_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsh_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsw_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsw_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsw_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vsw_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_b_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_h_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_w_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vse_v_d_mask, void, ptr, ptr, tl, env, i32)
DEF_HELPER_6(vlsb_v_b, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsb_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsb_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsb_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsh_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsh_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsh_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsw_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsw_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlse_v_b, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlse_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlse_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlse_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsbu_v_b, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsbu_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsbu_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlsbu_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlshu_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlshu_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlshu_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlswu_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlswu_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssb_v_b, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssb_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssb_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssb_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssh_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssh_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssh_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssw_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vssw_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vsse_v_b, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vsse_v_h, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vsse_v_w, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vsse_v_d, void, ptr, ptr, tl, tl, env, i32)
DEF_HELPER_6(vlxb_v_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxb_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxb_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxb_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxh_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxh_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxh_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxw_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxw_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxe_v_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxe_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxe_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxe_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxbu_v_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxbu_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxbu_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxbu_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxhu_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxhu_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxhu_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxwu_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vlxwu_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxb_v_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxb_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxb_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxb_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxh_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxh_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxh_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxw_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxw_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxe_v_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxe_v_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxe_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsxe_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_5(vlbff_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbff_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhff_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vleff_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vleff_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vleff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vleff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbuff_v_b, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbuff_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbuff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlbuff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhuff_v_h, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhuff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlhuff_v_d, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwuff_v_w, void, ptr, ptr, tl, env, i32)
DEF_HELPER_5(vlwuff_v_d, void, ptr, ptr, tl, env, i32)
#ifdef TARGET_RISCV64
DEF_HELPER_6(vamoswapw_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoswapd_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoaddw_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoaddd_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoxorw_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoxord_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoandw_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoandd_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoorw_v_d,   void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoord_v_d,   void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamominw_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomind_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxw_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxd_v_d,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamominuw_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamominud_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxuw_v_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxud_v_d, void, ptr, ptr, tl, ptr, env, i32)
#endif
DEF_HELPER_6(vamoswapw_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoaddw_v_w,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoxorw_v_w,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoandw_v_w,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamoorw_v_w,   void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamominw_v_w,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxw_v_w,  void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamominuw_v_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vamomaxuw_v_w, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vadd_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadd_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadd_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadd_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsub_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsub_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsub_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsub_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadd_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadd_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadd_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadd_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsub_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsub_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsub_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsub_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vrsub_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vrsub_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vrsub_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vrsub_vx_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vwaddu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwaddu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwaddu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwaddu_wv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_wv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_wv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_wv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_wv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsubu_wv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_wv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_wv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwadd_wv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_wv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_wv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwsub_wv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vwaddu_wx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwaddu_wx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwaddu_wx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_wx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_wx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsubu_wx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_wx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_wx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwadd_wx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_wx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_wx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vwsub_wx_w, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vadc_vvm_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadc_vvm_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadc_vvm_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadc_vvm_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsbc_vvm_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsbc_vvm_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsbc_vvm_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsbc_vvm_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmadc_vvm_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmadc_vvm_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmadc_vvm_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmadc_vvm_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsbc_vvm_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsbc_vvm_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsbc_vvm_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsbc_vvm_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vadc_vxm_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadc_vxm_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadc_vxm_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vadc_vxm_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsbc_vxm_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsbc_vxm_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsbc_vxm_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsbc_vxm_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmadc_vxm_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmadc_vxm_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmadc_vxm_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmadc_vxm_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsbc_vxm_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsbc_vxm_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsbc_vxm_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsbc_vxm_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vand_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vand_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vand_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vand_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vor_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vor_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vor_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vor_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vxor_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vxor_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vxor_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vxor_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vand_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vand_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vand_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vand_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vor_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vor_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vor_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vor_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vxor_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vxor_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vxor_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vxor_vx_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vsll_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsll_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsll_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsll_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsrl_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsrl_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsrl_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsrl_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsra_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsra_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsra_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsra_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vsll_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsll_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsll_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsll_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsrl_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsrl_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsrl_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsrl_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsra_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsra_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsra_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vsra_vx_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vnsrl_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsrl_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsrl_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsra_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsra_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsra_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vnsrl_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vnsrl_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vnsrl_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vnsra_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vnsra_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vnsra_vx_w, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vmseq_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmseq_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmseq_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmseq_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsne_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsne_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsne_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsne_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsltu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsltu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsltu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsltu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmslt_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmslt_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmslt_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmslt_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsleu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsleu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsleu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsleu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsle_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsle_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsle_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmsle_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmseq_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmseq_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmseq_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmseq_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsne_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsne_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsne_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsne_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsltu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsltu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsltu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsltu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmslt_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmslt_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmslt_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmslt_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsleu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsleu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsleu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsleu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsle_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsle_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsle_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsle_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgtu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgtu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgtu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgtu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgt_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgt_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgt_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmsgt_vx_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vminu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vminu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vminu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vminu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmin_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmin_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmin_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmin_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmaxu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmaxu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmaxu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmaxu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmax_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmax_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmax_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmax_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vminu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vminu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vminu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vminu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmin_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmin_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmin_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmin_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmaxu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmaxu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmaxu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmaxu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmax_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmax_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmax_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmax_vx_d, void, ptr, ptr, tl, ptr, env, i32)

DEF_HELPER_6(vmul_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmul_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmul_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmul_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulh_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulh_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulh_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulh_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vv_b, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vv_h, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vv_w, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vv_d, void, ptr, ptr, ptr, ptr, env, i32)
DEF_HELPER_6(vmul_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmul_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmul_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmul_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulh_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulh_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulh_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulh_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vx_b, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vx_h, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vx_w, void, ptr, ptr, tl, ptr, env, i32)
DEF_HELPER_6(vmulhsu_vx_d, void, ptr, ptr, tl, ptr, env, i32)
