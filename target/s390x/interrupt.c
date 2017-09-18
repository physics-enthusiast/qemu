/*
 * QEMU S/390 Interrupt support
 *
 * Copyright IBM Corp. 2012, 2014
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at your
 * option) any later version.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "kvm_s390x.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "sysemu/kvm.h"
#include "hw/s390x/ioinst.h"

/* Ensure to exit the TB after this call! */
void trigger_pgm_exception(CPUS390XState *env, uint32_t code, uint32_t ilen)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));

    cs->exception_index = EXCP_PGM;
    env->int_pgm_code = code;
    env->int_pgm_ilen = ilen;
}

static void tcg_s390_program_interrupt(CPUS390XState *env, uint32_t code,
                                       int ilen)
{
#ifdef CONFIG_TCG
    trigger_pgm_exception(env, code, ilen);
    cpu_loop_exit(CPU(s390_env_get_cpu(env)));
#else
    g_assert_not_reached();
#endif
}

void program_interrupt(CPUS390XState *env, uint32_t code, int ilen)
{
    S390CPU *cpu = s390_env_get_cpu(env);

    qemu_log_mask(CPU_LOG_INT, "program interrupt at %#" PRIx64 "\n",
                  env->psw.addr);

    if (kvm_enabled()) {
        kvm_s390_program_interrupt(cpu, code);
    } else if (tcg_enabled()) {
        tcg_s390_program_interrupt(env, code, ilen);
    } else {
        g_assert_not_reached();
    }
}

#if !defined(CONFIG_USER_ONLY)
void cpu_inject_ext(S390CPU *cpu, uint32_t code, uint32_t param,
                    uint64_t param64)
{
    CPUS390XState *env = &cpu->env;

    if (env->ext_index == MAX_EXT_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->ext_index++;
    assert(env->ext_index < MAX_EXT_QUEUE);

    env->ext_queue[env->ext_index].code = code;
    env->ext_queue[env->ext_index].param = param;
    env->ext_queue[env->ext_index].param64 = param64;

    env->pending_int |= INTERRUPT_EXT_FLOATING;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

void cpu_inject_clock_comparator(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    env->pending_int |= INTERRUPT_EXT_CLOCK_COMPARATOR;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

void cpu_inject_cpu_timer(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    env->pending_int |= INTERRUPT_EXT_CPU_TIMER;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

void cpu_inject_emergency_signal(S390CPU *cpu, uint16_t src_cpu_addr)
{
    CPUS390XState *env = &cpu->env;

    g_assert(src_cpu_addr < S390_MAX_CPUS);
    set_bit(src_cpu_addr, env->emergency_signals);

    env->pending_int |= INTERRUPT_EMERGENCY_SIGNAL;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

int cpu_inject_external_call(S390CPU *cpu, uint16_t src_cpu_addr)
{
    CPUS390XState *env = &cpu->env;

    g_assert(src_cpu_addr < S390_MAX_CPUS);
    if (env->pending_int & INTERRUPT_EXTERNAL_CALL) {
        return -EBUSY;
    }
    env->external_call_addr = src_cpu_addr;

    env->pending_int |= INTERRUPT_EXTERNAL_CALL;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
    return 0;
}

void cpu_inject_restart(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    if (kvm_enabled()) {
        kvm_s390_restart_interrupt(cpu);
        return;
    }

    env->pending_int |= INTERRUPT_RESTART;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

void cpu_inject_stop(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    if (kvm_enabled()) {
        kvm_s390_stop_interrupt(cpu);
        return;
    }

    env->pending_int |= INTERRUPT_STOP;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

static void cpu_inject_io(S390CPU *cpu, uint16_t subchannel_id,
                          uint16_t subchannel_number,
                          uint32_t io_int_parm, uint32_t io_int_word)
{
    CPUS390XState *env = &cpu->env;
    int isc = IO_INT_WORD_ISC(io_int_word);

    if (env->io_index[isc] == MAX_IO_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->io_index[isc]++;
    assert(env->io_index[isc] < MAX_IO_QUEUE);

    env->io_queue[env->io_index[isc]][isc].id = subchannel_id;
    env->io_queue[env->io_index[isc]][isc].nr = subchannel_number;
    env->io_queue[env->io_index[isc]][isc].parm = io_int_parm;
    env->io_queue[env->io_index[isc]][isc].word = io_int_word;

    env->pending_int |= INTERRUPT_IO;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

static void cpu_inject_crw_mchk(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    if (env->mchk_index == MAX_MCHK_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->mchk_index++;
    assert(env->mchk_index < MAX_MCHK_QUEUE);

    env->mchk_queue[env->mchk_index].type = 1;

    env->pending_int |= INTERRUPT_MCHK;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

/*
 * All of the following interrupts are floating, i.e. not per-vcpu.
 * We just need a dummy cpustate in order to be able to inject in the
 * non-kvm case.
 */
void s390_sclp_extint(uint32_t parm)
{
    if (kvm_enabled()) {
        kvm_s390_service_interrupt(parm);
    } else {
        S390CPU *dummy_cpu = s390_cpu_addr2state(0);

        cpu_inject_ext(dummy_cpu, EXT_SERVICE, parm, 0);
    }
}

void s390_io_interrupt(uint16_t subchannel_id, uint16_t subchannel_nr,
                       uint32_t io_int_parm, uint32_t io_int_word)
{
    if (kvm_enabled()) {
        kvm_s390_io_interrupt(subchannel_id, subchannel_nr, io_int_parm,
                              io_int_word);
    } else {
        S390CPU *dummy_cpu = s390_cpu_addr2state(0);

        cpu_inject_io(dummy_cpu, subchannel_id, subchannel_nr, io_int_parm,
                      io_int_word);
    }
}

void s390_crw_mchk(void)
{
    if (kvm_enabled()) {
        kvm_s390_crw_mchk();
    } else {
        S390CPU *dummy_cpu = s390_cpu_addr2state(0);

        cpu_inject_crw_mchk(dummy_cpu);
    }
}

#endif
