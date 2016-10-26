/*
 *  ioapic.c IOAPIC emulation logic
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 *
 *  Split the ioapic logic from apic.c
 *  Xiantao Zhang <xiantao.zhang@intel.com>
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
#include "qemu/error-report.h"
#include "monitor/monitor.h"
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/i386/apic.h"
#include "hw/i386/ioapic.h"
#include "hw/i386/ioapic_internal.h"
#include "include/hw/pci/msi.h"
#include "sysemu/kvm.h"
#include "target-i386/cpu.h"
#include "hw/i386/apic-msidef.h"
#include "hw/i386/x86-iommu.h"

//#define DEBUG_IOAPIC

#ifdef DEBUG_IOAPIC
#define DPRINTF(fmt, ...)                                       \
    do { printf("ioapic: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define APIC_DELIVERY_MODE_SHIFT 8
#define APIC_POLARITY_SHIFT 14
#define APIC_TRIG_MODE_SHIFT 15

static IOAPICCommonState *ioapics[MAX_IOAPICS];

/* global variable from ioapic_common.c */
extern int ioapic_no;

struct ioapic_entry_info {
    /* fields parsed from IOAPIC entries */
    uint8_t masked;
    uint8_t trig_mode;
    uint16_t dest_idx;
    uint8_t dest_mode;
    uint8_t delivery_mode;
    uint8_t vector;

    /* MSI message generated from above parsed fields */
    uint32_t addr;
    uint32_t data;
};

static void ioapic_entry_parse(uint64_t entry, struct ioapic_entry_info *info)
{
    memset(info, 0, sizeof(*info));
    info->masked = (entry >> IOAPIC_LVT_MASKED_SHIFT) & 1;
    info->trig_mode = (entry >> IOAPIC_LVT_TRIGGER_MODE_SHIFT) & 1;
    /*
     * By default, this would be dest_id[8] + reserved[8]. When IR
     * is enabled, this would be interrupt_index[15] +
     * interrupt_format[1]. This field never means anything, but
     * only used to generate corresponding MSI.
     */
    info->dest_idx = (entry >> IOAPIC_LVT_DEST_IDX_SHIFT) & 0xffff;
    info->dest_mode = (entry >> IOAPIC_LVT_DEST_MODE_SHIFT) & 1;
    info->delivery_mode = (entry >> IOAPIC_LVT_DELIV_MODE_SHIFT) \
        & IOAPIC_DM_MASK;
    if (info->delivery_mode == IOAPIC_DM_EXTINT) {
        info->vector = pic_read_irq(isa_pic);
    } else {
        info->vector = entry & IOAPIC_VECTOR_MASK;
    }

    info->addr = APIC_DEFAULT_ADDRESS | \
        (info->dest_idx << MSI_ADDR_DEST_IDX_SHIFT) | \
        (info->dest_mode << MSI_ADDR_DEST_MODE_SHIFT);
    info->data = (info->vector << MSI_DATA_VECTOR_SHIFT) | \
        (info->trig_mode << MSI_DATA_TRIGGER_SHIFT) | \
        (info->delivery_mode << MSI_DATA_DELIVERY_MODE_SHIFT);
}

static void ioapic_service(IOAPICCommonState *s)
{
    AddressSpace *ioapic_as = PC_MACHINE(qdev_get_machine())->ioapic_as;
    struct ioapic_entry_info info;
    uint8_t i;
    uint32_t mask;
    uint64_t entry;

    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        mask = 1 << i;
        if (s->irr & mask) {
            int coalesce = 0;

            entry = s->ioredtbl[i];
            ioapic_entry_parse(entry, &info);
            if (!info.masked) {
                if (info.trig_mode == IOAPIC_TRIGGER_EDGE) {
                    s->irr &= ~mask;
                } else {
                    coalesce = s->ioredtbl[i] & IOAPIC_LVT_REMOTE_IRR;
                    s->ioredtbl[i] |= IOAPIC_LVT_REMOTE_IRR;
                }

                if (coalesce) {
                    /* We are level triggered interrupts, and the
                     * guest should be still working on previous one,
                     * so skip it. */
                    continue;
                }

#ifdef CONFIG_KVM
                if (kvm_irqchip_is_split()) {
                    if (info.trig_mode == IOAPIC_TRIGGER_EDGE) {
                        kvm_set_irq(kvm_state, i, 1);
                        kvm_set_irq(kvm_state, i, 0);
                    } else {
                        kvm_set_irq(kvm_state, i, 1);
                    }
                    continue;
                }
#endif

                /* No matter whether IR is enabled, we translate
                 * the IOAPIC message into a MSI one, and its
                 * address space will decide whether we need a
                 * translation. */
                stl_le_phys(ioapic_as, info.addr, info.data);
            }
        }
    }
}

static void ioapic_set_irq(void *opaque, int vector, int level)
{
    IOAPICCommonState *s = opaque;

    /* ISA IRQs map to GSI 1-1 except for IRQ0 which maps
     * to GSI 2.  GSI maps to ioapic 1-1.  This is not
     * the cleanest way of doing it but it should work. */

    DPRINTF("%s: %s vec %x\n", __func__, level ? "raise" : "lower", vector);
    if (vector == 0) {
        vector = 2;
    }
    if (vector >= 0 && vector < IOAPIC_NUM_PINS) {
        uint32_t mask = 1 << vector;
        uint64_t entry = s->ioredtbl[vector];

        if (((entry >> IOAPIC_LVT_TRIGGER_MODE_SHIFT) & 1) ==
            IOAPIC_TRIGGER_LEVEL) {
            /* level triggered */
            if (level) {
                s->irr |= mask;
                if (!(entry & IOAPIC_LVT_REMOTE_IRR)) {
                    ioapic_service(s);
                }
            } else {
                s->irr &= ~mask;
            }
        } else {
            /* According to the 82093AA manual, we must ignore edge requests
             * if the input pin is masked. */
            if (level && !(entry & IOAPIC_LVT_MASKED)) {
                s->irr |= mask;
                ioapic_service(s);
            }
        }
    }
}

static void ioapic_update_kvm_routes(IOAPICCommonState *s)
{
#ifdef CONFIG_KVM
    int i;

    if (kvm_irqchip_is_split()) {
        for (i = 0; i < IOAPIC_NUM_PINS; i++) {
            MSIMessage msg;
            struct ioapic_entry_info info;
            ioapic_entry_parse(s->ioredtbl[i], &info);
            msg.address = info.addr;
            msg.data = info.data;
            kvm_irqchip_update_msi_route(kvm_state, i, msg, NULL);
        }
        kvm_irqchip_commit_routes(kvm_state);
    }
#endif
}

#ifdef CONFIG_KVM
static void ioapic_iec_notifier(void *private, bool global,
                                uint32_t index, uint32_t mask)
{
    IOAPICCommonState *s = (IOAPICCommonState *)private;
    /* For simplicity, we just update all the routes */
    ioapic_update_kvm_routes(s);
}
#endif

void ioapic_eoi_broadcast(int vector)
{
    IOAPICCommonState *s;
    uint64_t entry;
    int i, n;

    for (i = 0; i < MAX_IOAPICS; i++) {
        s = ioapics[i];
        if (!s) {
            continue;
        }
        for (n = 0; n < IOAPIC_NUM_PINS; n++) {
            entry = s->ioredtbl[n];
            if ((entry & IOAPIC_LVT_REMOTE_IRR)
                && (entry & IOAPIC_VECTOR_MASK) == vector) {
                s->ioredtbl[n] = entry & ~IOAPIC_LVT_REMOTE_IRR;
                if (!(entry & IOAPIC_LVT_MASKED) && (s->irr & (1 << n))) {
                    ioapic_service(s);
                }
            }
        }
    }
}

void ioapic_dump_state(Monitor *mon, const QDict *qdict)
{
    int i;

    for (i = 0; i < MAX_IOAPICS; i++) {
        if (ioapics[i] != 0) {
            ioapic_print_redtbl(mon, ioapics[i]);
        }
    }
}

static uint64_t
ioapic_mem_read(void *opaque, hwaddr addr, unsigned int size)
{
    IOAPICCommonState *s = opaque;
    int index;
    uint32_t val = 0;

    switch (addr & 0xff) {
    case IOAPIC_IOREGSEL:
        val = s->ioregsel;
        break;
    case IOAPIC_IOWIN:
        if (size != 4) {
            break;
        }
        switch (s->ioregsel) {
        case IOAPIC_REG_ID:
        case IOAPIC_REG_ARB:
            val = s->id << IOAPIC_ID_SHIFT;
            break;
        case IOAPIC_REG_VER:
            val = s->version |
                ((IOAPIC_NUM_PINS - 1) << IOAPIC_VER_ENTRIES_SHIFT);
            break;
        default:
            index = (s->ioregsel - IOAPIC_REG_REDTBL_BASE) >> 1;
            if (index >= 0 && index < IOAPIC_NUM_PINS) {
                if (s->ioregsel & 1) {
                    val = s->ioredtbl[index] >> 32;
                } else {
                    val = s->ioredtbl[index] & 0xffffffff;
                }
            }
        }
        DPRINTF("read: %08x = %08x\n", s->ioregsel, val);
        break;
    }
    return val;
}

/*
 * This is to satisfy the hack in Linux kernel. One hack of it is to
 * simulate clearing the Remote IRR bit of IOAPIC entry using the
 * following:
 *
 * "For IO-APIC's with EOI register, we use that to do an explicit EOI.
 * Otherwise, we simulate the EOI message manually by changing the trigger
 * mode to edge and then back to level, with RTE being masked during
 * this."
 *
 * (See linux kernel __eoi_ioapic_pin() comment in commit c0205701)
 *
 * This is based on the assumption that, Remote IRR bit will be
 * cleared by IOAPIC hardware when configured as edge-triggered
 * interrupts.
 *
 * Without this, level-triggered interrupts in IR mode might fail to
 * work correctly.
 */
static inline void
ioapic_fix_edge_remote_irr(uint64_t *entry)
{
    if (!(*entry & IOAPIC_LVT_TRIGGER_MODE)) {
        /* Edge-triggered interrupts, make sure remote IRR is zero */
        *entry &= ~((uint64_t)IOAPIC_LVT_REMOTE_IRR);
    }
}

static void
ioapic_mem_write(void *opaque, hwaddr addr, uint64_t val,
                 unsigned int size)
{
    IOAPICCommonState *s = opaque;
    int index;

    switch (addr & 0xff) {
    case IOAPIC_IOREGSEL:
        s->ioregsel = val;
        break;
    case IOAPIC_IOWIN:
        if (size != 4) {
            break;
        }
        DPRINTF("write: %08x = %08" PRIx64 "\n", s->ioregsel, val);
        switch (s->ioregsel) {
        case IOAPIC_REG_ID:
            s->id = (val >> IOAPIC_ID_SHIFT) & IOAPIC_ID_MASK;
            break;
        case IOAPIC_REG_VER:
        case IOAPIC_REG_ARB:
            break;
        default:
            index = (s->ioregsel - IOAPIC_REG_REDTBL_BASE) >> 1;
            if (index >= 0 && index < IOAPIC_NUM_PINS) {
                uint64_t ro_bits = s->ioredtbl[index] & IOAPIC_RO_BITS;
                if (s->ioregsel & 1) {
                    s->ioredtbl[index] &= 0xffffffff;
                    s->ioredtbl[index] |= (uint64_t)val << 32;
                } else {
                    s->ioredtbl[index] &= ~0xffffffffULL;
                    s->ioredtbl[index] |= val;
                }
                /* restore RO bits */
                s->ioredtbl[index] &= IOAPIC_RW_BITS;
                s->ioredtbl[index] |= ro_bits;
                ioapic_fix_edge_remote_irr(&s->ioredtbl[index]);
                ioapic_service(s);
            }
        }
        break;
    case IOAPIC_EOI:
        /* Explicit EOI is only supported for IOAPIC version 0x20 */
        if (size != 4 || s->version != 0x20) {
            break;
        }
        ioapic_eoi_broadcast(val);
        break;
    }

    ioapic_update_kvm_routes(s);
}

static const MemoryRegionOps ioapic_io_ops = {
    .read = ioapic_mem_read,
    .write = ioapic_mem_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ioapic_machine_done_notify(Notifier *notifier, void *data)
{
#ifdef CONFIG_KVM
    IOAPICCommonState *s = container_of(notifier, IOAPICCommonState,
                                        machine_done);

    if (kvm_irqchip_is_split()) {
        X86IOMMUState *iommu = x86_iommu_get_default();
        if (iommu) {
            /* Register this IOAPIC with IOMMU IEC notifier, so that
             * when there are IR invalidates, we can be notified to
             * update kernel IR cache. */
            x86_iommu_iec_register_notifier(iommu, ioapic_iec_notifier, s);
        }
    }
#endif
}

static void ioapic_realize(DeviceState *dev, Error **errp)
{
    IOAPICCommonState *s = IOAPIC_COMMON(dev);

    if (s->version != 0x11 && s->version != 0x20) {
        error_report("IOAPIC only supports version 0x11 or 0x20 "
                     "(default: 0x11).");
        exit(1);
    }

    memory_region_init_io(&s->io_memory, OBJECT(s), &ioapic_io_ops, s,
                          "ioapic", 0x1000);

    qdev_init_gpio_in(dev, ioapic_set_irq, IOAPIC_NUM_PINS);

    ioapics[ioapic_no] = s;
    s->machine_done.notify = ioapic_machine_done_notify;
    qemu_add_machine_init_done_notifier(&s->machine_done);
}

static Property ioapic_properties[] = {
    DEFINE_PROP_UINT8("version", IOAPICCommonState, version, 0x20),
    DEFINE_PROP_END_OF_LIST(),
};

static void ioapic_class_init(ObjectClass *klass, void *data)
{
    IOAPICCommonClass *k = IOAPIC_COMMON_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->realize = ioapic_realize;
    dc->reset = ioapic_reset_common;
    device_class_set_props(dc, ioapic_properties);
}

static const TypeInfo ioapic_info = {
    .name          = "ioapic",
    .parent        = TYPE_IOAPIC_COMMON,
    .instance_size = sizeof(IOAPICCommonState),
    .class_init    = ioapic_class_init,
};

static void ioapic_register_types(void)
{
    type_register_static(&ioapic_info);
}

type_init(ioapic_register_types)
