/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Virt system Controller
 */

#ifndef VIRT_CTRL_H
#define VIRT_CTRL_H

#define TYPE_VIRT_CTRL "virt-ctrl"
OBJECT_DECLARE_SIMPLE_TYPE(VirtCtrlState, VIRT_CTRL)

struct VirtCtrlState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t irq_enabled;

    MachineState *machine;
    char *fw_elf;
    char *fw_ramfs;
    uint32_t fw_bootinfo_size;
    uint8_t *fw_bootinfo;
};

#endif
