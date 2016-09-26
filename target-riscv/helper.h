/* Exceptions */
DEF_HELPER_2(raise_exception, noreturn, env, i32)
DEF_HELPER_1(raise_exception_debug, noreturn, env)
DEF_HELPER_3(raise_exception_mbadaddr, noreturn, env, i32, tl)

#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_3(mulhsu, TCG_CALL_NO_RWG_SE, tl, env, tl, tl)
#endif

/* Floating Point - fused */
DEF_HELPER_FLAGS_5(fmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fnmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fnmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fnmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)
DEF_HELPER_FLAGS_5(fnmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64, i64)

/* Floating Point - Single Precision */
DEF_HELPER_FLAGS_4(fadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmul_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fdiv_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_3(fsgnj_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsgnjn_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsgnjx_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmin_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmax_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsqrt_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fle_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(flt_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(feq_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(fcvt_w_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(fcvt_wu_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_3(fcvt_l_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fcvt_lu_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
#endif
DEF_HELPER_FLAGS_3(fcvt_s_w, TCG_CALL_NO_RWG, i64, env, tl, i64)
DEF_HELPER_FLAGS_3(fcvt_s_wu, TCG_CALL_NO_RWG, i64, env, tl, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_3(fcvt_s_l, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fcvt_s_lu, TCG_CALL_NO_RWG, i64, env, i64, i64)
#endif
DEF_HELPER_FLAGS_2(fclass_s, TCG_CALL_NO_RWG, tl, env, i64)
