/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Host specific cpu indentification for ppc.
 */

#include "qemu/osdep.h"
#include "host/cpuinfo.h"

#ifdef CONFIG_GETAUXVAL
# include <sys/auxv.h>
#else
# include <asm/cputable.h>
# include "elf.h"
#endif

unsigned cpuinfo;

/* Called both as constructor and (possibly) via other constructors. */
unsigned __attribute__((constructor)) cpuinfo_init(void)
{
    unsigned info = cpuinfo;
    unsigned long hwcap, hwcap2;

    if (info) {
        return info;
    }

    hwcap = qemu_getauxval(AT_HWCAP);
    hwcap2 = qemu_getauxval(AT_HWCAP2);
    info = CPUINFO_ALWAYS;

    if (hwcap & PPC_FEATURE_ARCH_2_06) {
        info |= CPUINFO_V2_06;
    }
    if (hwcap2 & PPC_FEATURE2_ARCH_2_07) {
        info |= CPUINFO_V2_07;
    }
    if (hwcap2 & PPC_FEATURE2_ARCH_3_00) {
        info |= CPUINFO_V3_00;
    }
    if (hwcap2 & PPC_FEATURE2_ARCH_3_1) {
        info |= CPUINFO_V3_10;
    }
    if (hwcap2 & PPC_FEATURE2_HAS_ISEL) {
        info |= CPUINFO_ISEL;
    }
    if (hwcap & PPC_FEATURE_HAS_ALTIVEC) {
        info |= CPUINFO_ALTIVEC;
        /* We only care about the portion of VSX that overlaps Altivec. */
        if (hwcap & PPC_FEATURE_HAS_VSX) {
            info |= CPUINFO_VSX;
        }
    }

    cpuinfo = info;
    return info;
}
