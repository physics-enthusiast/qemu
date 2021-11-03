/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any
 * later version. See the COPYING file in the top-level directory.
 */

#ifndef XTENSA_TARGET_ELF_H
#define XTENSA_TARGET_ELF_H

static inline const char *cpu_get_model(uint32_t eflags)
{
    return XTENSA_DEFAULT_CPU_MODEL;
}

#endif
