/*
 * OpenRISC MMU.
 *
 * Copyright (c) 2011-2012 Jia Liu <proljc@gmail.com>
 *                         Zhizhou Zhang <etouzh@gmail.com>
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
#include "cpu.h"
#include "exec/exec-all.h"
#include "qemu-common.h"
#include "exec/gdbstub.h"
#include "qemu/host-utils.h"
#ifndef CONFIG_USER_ONLY
#include "hw/loader.h"
#endif

#ifndef CONFIG_USER_ONLY
static inline void get_phys_nommu(hwaddr *phys_addr, int *prot,
                                  target_ulong address)
{
    *phys_addr = address;
    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
}

static int get_phys_mmu(OpenRISCCPU *cpu, hwaddr *phys_addr, int *prot,
                        target_ulong addr, int need, bool super)
{
    int idx = (addr >> TARGET_PAGE_BITS) & TLB_MASK;
    uint32_t imr = cpu->env.tlb.itlb[idx].mr;
    uint32_t itr = cpu->env.tlb.itlb[idx].tr;
    uint32_t dmr = cpu->env.tlb.dtlb[idx].mr;
    uint32_t dtr = cpu->env.tlb.dtlb[idx].tr;
    int right, match, valid;

    /* If the ITLB and DTLB indexes map to the same page, we want to
       load all permissions all at once.  If the destination pages do
       not match, zap the one we don't need.  */
    if (unlikely((itr ^ dtr) & TARGET_PAGE_MASK)) {
        if (need & PAGE_EXEC) {
            dmr = dtr = 0;
        } else {
            imr = itr = 0;
        }
    }

    /* Check if either of the entries matches the source address.  */
    match  = (imr ^ addr) & TARGET_PAGE_MASK ? 0 : PAGE_EXEC;
    match |= (dmr ^ addr) & TARGET_PAGE_MASK ? 0 : PAGE_READ | PAGE_WRITE;

    /* Check if either of the entries is valid.  */
    valid  = imr & 1 ? PAGE_EXEC : 0;
    valid |= dmr & 1 ? PAGE_READ | PAGE_WRITE : 0;
    valid &= match;

    /* Collect the permissions from the entries.  */
    right  = itr & (super ? SXE : UXE) ? PAGE_EXEC : 0;
    right |= dtr & (super ? SRE : URE) ? PAGE_READ : 0;
    right |= dtr & (super ? SWE : UWE) ? PAGE_WRITE : 0;
    right &= valid;

    /* Note that above we validated that itr and dtr match on page.
       So oring them together changes nothing without having to
       check which one we needed.  We also want to store to these
       variables even on failure, as it avoids compiler warnings.  */
    *phys_addr = ((itr | dtr) & TARGET_PAGE_MASK) | (addr & ~TARGET_PAGE_MASK);
    *prot = right;

    qemu_log_mask(CPU_LOG_MMU,
                  "MMU lookup: need %d match %d valid %d right %d -> %s\n",
                  need, match, valid, right, (need & right) ? "OK" : "FAIL");

    /* Check the collective permissions are present.  */
    if (likely(need & right)) {
        return 0;  /* success! */
    }

    /* Determine what kind of failure we have.  */
    if (need & valid) {
        return need & PAGE_EXEC ? EXCP_IPF : EXCP_DPF;
    } else {
        return need & PAGE_EXEC ? EXCP_ITLBMISS : EXCP_DTLBMISS;
    }
}
#endif

static void raise_mmu_exception(OpenRISCCPU *cpu, target_ulong address,
                                int exception)
{
    CPUState *cs = CPU(cpu);

    cs->exception_index = exception;
    cpu->env.eear = address;
    cpu->env.lock_addr = -1;
}

int openrisc_cpu_handle_mmu_fault(CPUState *cs, vaddr address, int size,
                                  int rw, int mmu_idx)
{
#ifdef CONFIG_USER_ONLY
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);
    raise_mmu_exception(cpu, address, EXCP_DPF);
    return 1;
#else
    g_assert_not_reached();
#endif
}

#ifndef CONFIG_USER_ONLY
hwaddr openrisc_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);
    int prot, excp, sr = cpu->env.sr;
    hwaddr phys_addr;

    switch (sr & (SR_DME | SR_IME)) {
    case SR_DME | SR_IME:
        /* The mmu is definitely enabled.  */
        excp = get_phys_mmu(cpu, &phys_addr, &prot, addr,
                            PROT_EXEC | PROT_READ | PROT_WRITE,
                            (sr & SR_SM) != 0);
        return excp ? -1 : phys_addr;

    default:
        /* The mmu is partially enabled, and we don't really have
           a "real" access type.  Begin by trying the mmu, but if
           that fails try again without.  */
        excp = get_phys_mmu(cpu, &phys_addr, &prot, addr,
                            PROT_EXEC | PROT_READ | PROT_WRITE,
                            (sr & SR_SM) != 0);
        if (!excp) {
            return phys_addr;
        }
        /* fallthru */

    case 0:
        /* The mmu is definitely disabled; lookups never fail.  */
        get_phys_nommu(&phys_addr, &prot, addr);
        return phys_addr;
    }
}

void tlb_fill(CPUState *cs, target_ulong addr, int size,
              MMUAccessType access_type, int mmu_idx, uintptr_t retaddr)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);
    int prot, excp;
    hwaddr phys_addr;

    if (mmu_idx == MMU_NOMMU_IDX) {
        /* The mmu is disabled; lookups never fail.  */
        get_phys_nommu(&phys_addr, &prot, addr);
        excp = 0;
    } else {
        bool super = mmu_idx == MMU_SUPERVISOR_IDX;
        int need = (access_type == MMU_INST_FETCH ? PROT_EXEC
                    : access_type == MMU_DATA_STORE ? PROT_WRITE
                    : PROT_READ);
        excp = get_phys_mmu(cpu, &phys_addr, &prot, addr, need, super);
    }

    if (unlikely(excp)) {
        raise_mmu_exception(cpu, addr, excp);
        cpu_loop_exit_restore(cs, retaddr);
    }

    tlb_set_page(cs, addr & TARGET_PAGE_MASK,
                 phys_addr & TARGET_PAGE_MASK, prot,
                 mmu_idx, TARGET_PAGE_SIZE);
}
#endif
