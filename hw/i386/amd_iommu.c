/*
 * QEMU emulation of AMD IOMMU (AMD-Vi)
 *
 * Copyright (C) 2011 Eduard - Gabriel Munteanu
 * Copyright (C) 2015 David Kiarie, <davidkiarie4@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Cache implementation inspired by hw/i386/intel_iommu.c
 */
#include "qemu/osdep.h"
#include "hw/i386/amd_iommu.h"
#include "trace.h"

/* used AMD-Vi MMIO registers */
const char *amdvi_mmio_low[] = {
    "AMDVI_MMIO_DEVTAB_BASE",
    "AMDVI_MMIO_CMDBUF_BASE",
    "AMDVI_MMIO_EVTLOG_BASE",
    "AMDVI_MMIO_CONTROL",
    "AMDVI_MMIO_EXCL_BASE",
    "AMDVI_MMIO_EXCL_LIMIT",
    "AMDVI_MMIO_EXT_FEATURES",
    "AMDVI_MMIO_PPR_BASE",
    "UNHANDLED"
};
const char *amdvi_mmio_high[] = {
    "AMDVI_MMIO_COMMAND_HEAD",
    "AMDVI_MMIO_COMMAND_TAIL",
    "AMDVI_MMIO_EVTLOG_HEAD",
    "AMDVI_MMIO_EVTLOG_TAIL",
    "AMDVI_MMIO_STATUS",
    "AMDVI_MMIO_PPR_HEAD",
    "AMDVI_MMIO_PPR_TAIL",
    "UNHANDLED"
};
struct AMDVIAddressSpace {
    uint8_t bus_num;            /* bus number                           */
    uint8_t devfn;              /* device function                      */
    AMDVIState *iommu_state;    /* AMDVI - one per machine              */
    MemoryRegion iommu;         /* Device's address translation region  */
    MemoryRegion iommu_ir;      /* Device's interrupt remapping region  */
    AddressSpace as;            /* device's corresponding address space */
};

/* AMDVI cache entry */
typedef struct AMDVIIOTLBEntry {
    uint16_t domid;             /* assigned domain id  */
    uint16_t devid;             /* device owning entry */
    uint64_t perms;             /* access permissions  */
    uint64_t translated_addr;   /* translated address  */
    uint64_t page_mask;         /* physical page size  */
} AMDVIIOTLBEntry;

/* serialize IOMMU command processing */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;               /* command type           */
    uint64_t reserved:8;
    uint64_t store_addr:49;        /* addr to write          */
    uint64_t completion_flush:1;   /* allow more executions  */
    uint64_t completion_int:1;     /* set MMIOWAITINT        */
    uint64_t completion_store:1;   /* write data to address  */
#else
    uint64_t completion_store:1;
    uint64_t completion_int:1;
    uint64_t completion_flush:1;
    uint64_t store_addr:49;
    uint64_t reserved:8;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */
    uint64_t store_data;           /* data to write          */
} CMDCompletionWait;

/* invalidate internal caches for devid */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t devid:16;             /* device to invalidate   */
    uint64_t reserved_1:44;
    uint64_t type:4;               /* command type           */
#else
    uint64_t devid:16;
    uint64_t reserved_1:44;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */
    uint64_t reserved_2;
} CMDInvalDevEntry;

/* invalidate a range of entries in IOMMU translation cache for devid */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;               /* command type           */
    uint64_t reserved_2:12;
    uint64_t domid:16;             /* domain to inval for    */
    uint64_t reserved_1:12;
    uint64_t pasid:20;
#else
    uint64_t pasid:20;
    uint64_t reserved_1:12;
    uint64_t domid:16;
    uint64_t reserved_2:12;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */

#ifdef HOST_WORDS_BIGENDIAN
    uint64_t address:51;          /* address to invalidate   */
    uint64_t reserved_3:10;
    uint64_t guest:1;             /* G/N invalidation        */
    uint64_t pde:1;               /* invalidate cached ptes  */
    uint64_t size:1               /* size of invalidation    */
#else
    uint64_t size:1;
    uint64_t pde:1;
    uint64_t guest:1;
    uint64_t reserved_3:10;
    uint64_t address:51;
#endif /* __BIG_ENDIAN_BITFIELD */
} CMDInvalIommuPages;

/* inval specified address for devid from remote IOTLB */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;            /* command type        */
    uint64_t pasid_19_6:4;
    uint64_t pasid_7_0:8;
    uint64_t queuid:16;
    uint64_t maxpend:8;
    uint64_t pasid_15_8;
    uint64_t devid:16;         /* related devid        */
#else
    uint64_t devid:16;
    uint64_t pasid_15_8:8;
    uint64_t maxpend:8;
    uint64_t queuid:16;
    uint64_t pasid_7_0:8;
    uint64_t pasid_19_6:4;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */

#ifdef HOST_WORDS_BIGENDIAN
    uint64_t address:52;       /* invalidate addr      */
    uint64_t reserved_2:9;
    uint64_t guest:1;          /* G/N invalidate       */
    uint64_t reserved_1:1;
    uint64_t size:1;           /* size of invalidation */
#else
    uint64_t size:1;
    uint64_t reserved_1:1;
    uint64_t guest:1;
    uint64_t reserved_2:9;
    uint64_t address:52;
#endif /* __BIG_ENDIAN_BITFIELD */
} CMDInvalIOTLBPages;

/* invalidate all cached interrupt info for devid */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;          /* command type        */
    uint64_t reserved_1:44;
    uint64_t devid:16;        /* related devid       */
#else
    uint64_t devid:16;
    uint64_t reserved_1:44;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */
    uint64_t reserved_2;
} CMDInvalIntrTable;

/* load adddress translation info for devid into translation cache */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;          /* command type       */
    uint64_t reserved_2:8;
    uint64_t pasid_19_0:20;
    uint64_t pfcount_7_0:8;
    uint64_t reserved_1:8;
    uint64_t devid:16;        /* related devid      */
#else
    uint64_t devid:16;
    uint64_t reserved_1:8;
    uint64_t pfcount_7_0:8;
    uint64_t pasid_19_0:20;
    uint64_t reserved_2:8;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */

#ifdef HOST_WORDS_BIGENDIAN
    uint64_t address:52;     /* invalidate address       */
    uint64_t reserved_5:7;
    uint64_t inval:1;        /* inval matching entries   */
    uint64_t reserved_4:1;
    uint64_t guest:1;        /* G/N invalidate           */
    uint64_t reserved_3:1;
    uint64_t size:1;         /* prefetched page size     */
#else
    uint64_t size:1;
    uint64_t reserved_3:1;
    uint64_t guest:1;
    uint64_t reserved_4:1;
    uint64_t inval:1;
    uint64_t reserved_5:7;
    uint64_t address:52;
#endif /* __BIG_ENDIAN_BITFIELD */
} CMDPrefetchPages;

/* clear all address translation/interrupt remapping caches */
typedef struct QEMU_PACKED {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t type:4;              /* command type       */
    uint64_t reserved_1:60;
#else
    uint64_t reserved_1:60;
    uint64_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */
    uint64_t reserved_2;
} CMDInvalIommuAll;

/* issue a PCIe completion packet for devid */
typedef struct QEMU_PACKED {
    uint32_t reserved_1:16;
    uint32_t devid:16;

#ifdef HOST_WORDS_BIGENDIAN
    uint32_t type:4;              /* command type       */
    uint32_t reserved_2:8;
    uint32_t pasid_19_0:20
#else
    uint32_t pasid_19_0:20;
    uint32_t reserved_2:8;
    uint32_t type:4;
#endif /* __BIG_ENDIAN_BITFIELD */

#ifdef HOST_WORDS_BIGENDIAN
    uint32_t reserved_3:29;
    uint32_t guest:1;
    uint32_t reserved_4:2;
#else
    uint32_t reserved_3:2;
    uint32_t guest:1;
    uint32_t reserved_4:29;
#endif /* __BIG_ENDIAN_BITFIELD */

    uint32_t completion_tag:16;  /* PCIe PRI information */
    uint32_t reserved_5:16;
} CMDCompletePPR;

/* configure MMIO registers at startup/reset */
static void amdvi_set_quad(AMDVIState *s, hwaddr addr, uint64_t val,
                           uint64_t romask, uint64_t w1cmask)
{
    stq_le_p(&s->mmior[addr], val);
    stq_le_p(&s->romask[addr], romask);
    stq_le_p(&s->w1cmask[addr], w1cmask);
}

static uint16_t amdvi_readw(AMDVIState *s, hwaddr addr)
{
    return lduw_le_p(&s->mmior[addr]);
}

static uint32_t amdvi_readl(AMDVIState *s, hwaddr addr)
{
    return ldl_le_p(&s->mmior[addr]);
}

static uint64_t amdvi_readq(AMDVIState *s, hwaddr addr)
{
    return ldq_le_p(&s->mmior[addr]);
}

/* internal write */
static void amdvi_writeq_raw(AMDVIState *s, uint64_t val, hwaddr addr)
{
    stq_le_p(&s->mmior[addr], val);
}

/* external write */
static void amdvi_writew(AMDVIState *s, hwaddr addr, uint16_t val)
{
    uint16_t romask = lduw_le_p(&s->romask[addr]);
    uint16_t w1cmask = lduw_le_p(&s->w1cmask[addr]);
    uint16_t oldval = lduw_le_p(&s->mmior[addr]);
    stw_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

static void amdvi_writel(AMDVIState *s, hwaddr addr, uint32_t val)
{
    uint32_t romask = ldl_le_p(&s->romask[addr]);
    uint32_t w1cmask = ldl_le_p(&s->w1cmask[addr]);
    uint32_t oldval = ldl_le_p(&s->mmior[addr]);
    stl_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

static void amdvi_writeq(AMDVIState *s, hwaddr addr, uint64_t val)
{
    uint64_t romask = ldq_le_p(&s->romask[addr]);
    uint64_t w1cmask = ldq_le_p(&s->w1cmask[addr]);
    uint32_t oldval = ldq_le_p(&s->mmior[addr]);
    stq_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

/* OR a 64-bit register with a 64-bit value */
static bool amdvi_test_mask(AMDVIState *s, hwaddr addr, uint64_t val)
{
    return amdvi_readq(s, addr) | val;
}

/* OR a 64-bit register with a 64-bit value storing result in the register */
static void amdvi_assign_orq(AMDVIState *s, hwaddr addr, uint64_t val)
{
    amdvi_writeq_raw(s, addr, amdvi_readq(s, addr) | val);
}

/* AND a 64-bit register with a 64-bit value storing result in the register */
static void amdvi_assign_andq(AMDVIState *s, hwaddr addr, uint64_t val)
{
   amdvi_writeq_raw(s, addr, amdvi_readq(s, addr) & val);
}

static void amdvi_generate_msi_interrupt(AMDVIState *s)
{
    MSIMessage msg;
    MemTxAttrs attrs;

    attrs.requester_id = pci_requester_id(&s->pci.dev);

    if (msi_enabled(&s->pci.dev)) {
        msg = msi_get_message(&s->pci.dev, 0);
        address_space_stl_le(&address_space_memory, msg.address, msg.data,
                             attrs, NULL);
    }
}

static void amdvi_log_event(AMDVIState *s, uint64_t *evt)
{
    /* event logging not enabled */
    if (!s->evtlog_enabled || amdvi_test_mask(s, AMDVI_MMIO_STATUS,
        AMDVI_MMIO_STATUS_EVT_OVF)) {
        return;
    }

    /* event log buffer full */
    if (s->evtlog_tail >= s->evtlog_len) {
        amdvi_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_EVT_OVF);
        /* generate interrupt */
        amdvi_generate_msi_interrupt(s);
        return;
    }

    if (dma_memory_write(&address_space_memory, s->evtlog + s->evtlog_tail,
        &evt, AMDVI_EVENT_LEN)) {
        trace_amdvi_evntlog_fail(s->evtlog, s->evtlog_tail);
    }

    s->evtlog_tail += AMDVI_EVENT_LEN;
    amdvi_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_COMP_INT);
    amdvi_generate_msi_interrupt(s);
}

static void amdvi_setevent_bits(uint64_t *buffer, uint64_t value, int start,
                                int length)
{
    int index = start / 64, bitpos = start % 64;
    uint64_t mask = ((1 << length) - 1) << bitpos;
    buffer[index] &= ~mask;
    buffer[index] |= (value << bitpos) & mask;
}
/*
 * AMDVi event structure
 *    0:15   -> DeviceID
 *    55:63  -> event type + miscellaneous info
 *    63:127 -> related address
 */
static void amdvi_encode_event(uint64_t *evt, uint16_t devid, uint64_t addr,
                               uint16_t info)
{
    amdvi_setevent_bits(evt, devid, 0, 16);
    amdvi_setevent_bits(evt, info, 55, 8);
    amdvi_setevent_bits(evt, addr, 63, 64);
}
/* log an error encountered during a page walk
 *
 * @addr: virtual address in translation request
 */
static void amdvi_page_fault(AMDVIState *s, uint16_t devid,
                             hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_IOPF_I | AMDVI_EVENT_IOPF;
    amdvi_encode_event(evt, devid, addr, info);
    amdvi_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci.dev.config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/*
 * log a master abort accessing device table
 *  @devtab : address of device table entry
 *  @info : error flags
 */
static void amdvi_log_devtab_error(AMDVIState *s, uint16_t devid,
                                   hwaddr devtab, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_DEV_TAB_HW_ERROR;

    amdvi_encode_event(evt, devid, devtab, info);
    amdvi_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci.dev.config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/* log an event trying to access command buffer
 *   @addr : address that couldn't be accessed
 */
static void amdvi_log_command_error(AMDVIState *s, hwaddr addr)
{
    uint64_t evt[4], info = AMDVI_EVENT_COMMAND_HW_ERROR;

    amdvi_encode_event(evt, 0, addr, info);
    amdvi_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci.dev.config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/* log an illegal comand event
 *   @addr : address of illegal command
 */
static void amdvi_log_illegalcom_error(AMDVIState *s, uint16_t info,
                                       hwaddr addr)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_ILLEGAL_COMMAND_ERROR;
    amdvi_encode_event(evt, 0, addr, info);
    amdvi_log_event(s, evt);
}
/* log an error accessing device table
 *
 *  @devid : device owning the table entry
 *  @devtab : address of device table entry
 *  @info : error flags
 */
static void amdvi_log_illegaldevtab_error(AMDVIState *s, uint16_t devid,
                                          hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_ILLEGAL_DEVTAB_ENTRY;
    amdvi_encode_event(evt, devid, addr, info);
    amdvi_log_event(s, evt);
}
/* log an error accessing a PTE entry
 * @addr : address that couldn't be accessed
 */
static void amdvi_log_pagetab_error(AMDVIState *s, uint16_t devid,
                                    hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_PAGE_TAB_HW_ERROR;
    amdvi_encode_event(evt, devid, addr, info);
    amdvi_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci.dev.config + PCI_STATUS,
             PCI_STATUS_SIG_TARGET_ABORT);
}

static gboolean amdvi_uint64_equal(gconstpointer v1, gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}

static guint amdvi_uint64_hash(gconstpointer v)
{
    return (guint)*(const uint64_t *)v;
}

static AMDVIIOTLBEntry *amdvi_iotlb_lookup(AMDVIState *s, hwaddr addr,
                                           uint64_t devid)
{
    uint64_t key = (addr >> AMDVI_PAGE_SHIFT_4K) |
                   ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
    return g_hash_table_lookup(s->iotlb, &key);
}

static void amdvi_iotlb_reset(AMDVIState *s)
{
    assert(s->iotlb);
    trace_amdvi_iotlb_reset();
    g_hash_table_remove_all(s->iotlb);
}

static gboolean amdvi_iotlb_remove_by_devid(gpointer key, gpointer value,
                                            gpointer user_data)
{
    AMDVIIOTLBEntry *entry = (AMDVIIOTLBEntry *)value;
    uint16_t devid = *(uint16_t *)user_data;
    return entry->devid == devid;
}

static void amdvi_iotlb_remove_page(AMDVIState *s, hwaddr addr,
                                    uint64_t devid)
{
    uint64_t key = (addr >> AMDVI_PAGE_SHIFT_4K) |
                   ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
    g_hash_table_remove(s->iotlb, &key);
}

static void amdvi_update_iotlb(AMDVIState *s, uint16_t devid,
                               uint64_t gpa, IOMMUTLBEntry to_cache,
                               uint16_t domid)
{
    AMDVIIOTLBEntry *entry = g_malloc(sizeof(*entry));
    uint64_t *key = g_malloc(sizeof(key));
    uint64_t gfn = gpa >> AMDVI_PAGE_SHIFT_4K;

    /* don't cache erroneous translations */
    if (to_cache.perm != IOMMU_NONE) {
        trace_amdvi_cache_update(domid, PCI_BUS_NUM(devid), PCI_SLOT(devid),
                PCI_FUNC(devid), gpa, to_cache.translated_addr);

        if (g_hash_table_size(s->iotlb) >= AMDVI_IOTLB_MAX_SIZE) {
            amdvi_iotlb_reset(s);
        }

        entry->domid = domid;
        entry->perms = to_cache.perm;
        entry->translated_addr = to_cache.translated_addr;
        entry->page_mask = to_cache.addr_mask;
        *key = gfn | ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
        g_hash_table_replace(s->iotlb, key, entry);
    }
}

static void amdvi_completion_wait(AMDVIState *s, CMDCompletionWait *wait)
{
    /* pad the last 3 bits */
    hwaddr addr = le64_to_cpu(wait->store_addr) << 3;
    uint64_t data = le64_to_cpu(wait->store_data);

    if (wait->reserved) {
        amdvi_log_illegalcom_error(s, wait->type, s->cmdbuf + s->cmdbuf_head);
    }
    if (wait->completion_store) {
        if (dma_memory_write(&address_space_memory, addr, &data,
            AMDVI_COMPLETION_DATA_SIZE)) {
            trace_amdvi_completion_wait_fail(addr);
        }
    }
    /* set completion interrupt */
    if (wait->completion_int) {
        amdvi_test_mask(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_COMP_INT);
        /* generate interrupt */
        amdvi_generate_msi_interrupt(s);
    }
    trace_amdvi_completion_wait(addr, data);
}

/* log error without aborting since linux seems to be using reserved bits */
static void amdvi_inval_devtab_entry(AMDVIState *s, void *cmd)
{
    CMDInvalIntrTable *inval = (CMDInvalIntrTable *)cmd;
    /* This command should invalidate internal caches of which there isn't */
    if (inval->reserved_1 || inval->reserved_2) {
        amdvi_log_illegalcom_error(s, inval->type, s->cmdbuf + s->cmdbuf_head);
    }
    trace_amdvi_devtab_inval(PCI_BUS_NUM(inval->devid), PCI_SLOT(inval->devid),
            PCI_FUNC(inval->devid));
}

static void amdvi_complete_ppr(AMDVIState *s, void *cmd)
{
    CMDCompletePPR *pprcomp = (CMDCompletePPR *)cmd;

    if (pprcomp->reserved_1 || pprcomp->reserved_2 || pprcomp->reserved_3 ||
        pprcomp->reserved_4 || pprcomp->reserved_5) {
        amdvi_log_illegalcom_error(s, pprcomp->type, s->cmdbuf +
                s->cmdbuf_head);
    }
    trace_amdvi_ppr_exec();
}

static void amdvi_inval_all(AMDVIState *s, CMDInvalIommuAll *inval)
{
    if (inval->reserved_2 || inval->reserved_1) {
        amdvi_log_illegalcom_error(s, inval->type, s->cmdbuf + s->cmdbuf_head);
    }

    amdvi_iotlb_reset(s);
    trace_amdvi_all_inval();
}

static gboolean amdvi_iotlb_remove_by_domid(gpointer key, gpointer value,
                                            gpointer user_data)
{
    AMDVIIOTLBEntry *entry = (AMDVIIOTLBEntry *)value;
    uint16_t domid = *(uint16_t *)user_data;
    return entry->domid == domid;
}

/* we don't have devid - we can't remove pages by address */
static void amdvi_inval_pages(AMDVIState *s, CMDInvalIommuPages *inval)
{
    uint16_t domid = inval->domid;

    if (inval->reserved_1 || inval->reserved_2 || inval->reserved_3) {
        amdvi_log_illegalcom_error(s, inval->type, s->cmdbuf + s->cmdbuf_head);
    }

    g_hash_table_foreach_remove(s->iotlb, amdvi_iotlb_remove_by_domid,
                                &domid);
    trace_amdvi_pages_inval(inval->domid);
}

static void amdvi_prefetch_pages(AMDVIState *s, CMDPrefetchPages *prefetch)
{
    if (prefetch->reserved_1 || prefetch->reserved_2 || prefetch->reserved_3
        || prefetch->reserved_4 || prefetch->reserved_5) {
        amdvi_log_illegalcom_error(s, prefetch->type, s->cmdbuf +
                s->cmdbuf_head);
    }
    trace_amdvi_prefetch_pages();
}

static void amdvi_inval_inttable(AMDVIState *s, CMDInvalIntrTable *inval)
{
    if (inval->reserved_1 || inval->reserved_2) {
        amdvi_log_illegalcom_error(s, inval->type, s->cmdbuf + s->cmdbuf_head);
        return;
    }
    trace_amdvi_intr_inval();
}

/* FIXME: Try to work with the specified size instead of all the pages
 * when the S bit is on
 */
static void iommu_inval_iotlb(AMDVIState *s, CMDInvalIOTLBPages *inval)
{
    uint16_t devid = inval->devid;

    if (inval->reserved_1 || inval->reserved_2) {
        amdvi_log_illegalcom_error(s, inval->type, s->cmdbuf + s->cmdbuf_head);
        return;
    }

    if (inval->size) {
        g_hash_table_foreach_remove(s->iotlb, amdvi_iotlb_remove_by_devid,
                                    &devid);
    } else {
        amdvi_iotlb_remove_page(s, ((uint64_t)inval->address) << 12,
                                inval->devid);
    }
    trace_amdvi_iotlb_inval();
}

/* not honouring reserved bits is regarded as an illegal command */
static void amdvi_cmdbuf_exec(AMDVIState *s)
{
    CMDCompletionWait cmd;

    if (dma_memory_read(&address_space_memory, s->cmdbuf + s->cmdbuf_head,
        &cmd, AMDVI_COMMAND_SIZE)) {
        trace_amdvi_command_read_fail(s->cmdbuf, s->cmdbuf_head);
        amdvi_log_command_error(s, s->cmdbuf + s->cmdbuf_head);
        return;
    }

    switch (cmd.type) {
    case AMDVI_CMD_COMPLETION_WAIT:
        amdvi_completion_wait(s, (CMDCompletionWait *)&cmd);
        break;
    case AMDVI_CMD_INVAL_DEVTAB_ENTRY:
        amdvi_inval_devtab_entry(s, (CMDInvalDevEntry *)&cmd);
        break;
    case AMDVI_CMD_INVAL_AMDVI_PAGES:
        amdvi_inval_pages(s, (CMDInvalIommuPages *)&cmd);
        break;
    case AMDVI_CMD_INVAL_IOTLB_PAGES:
        iommu_inval_iotlb(s, (CMDInvalIOTLBPages *)&cmd);
        break;
    case AMDVI_CMD_INVAL_INTR_TABLE:
        amdvi_inval_inttable(s, (CMDInvalIntrTable *)&cmd);
        break;
    case AMDVI_CMD_PREFETCH_AMDVI_PAGES:
        amdvi_prefetch_pages(s, (CMDPrefetchPages *)&cmd);
        break;
    case AMDVI_CMD_COMPLETE_PPR_REQUEST:
        amdvi_complete_ppr(s, (CMDCompletePPR *)&cmd);
        break;
    case AMDVI_CMD_INVAL_AMDVI_ALL:
        amdvi_inval_all(s, (CMDInvalIommuAll *)&cmd);
        break;
    default:
        trace_amdvi_unhandled_command(cmd.type);
        /* log illegal command */
        amdvi_log_illegalcom_error(s, cmd.type,
                                   s->cmdbuf + s->cmdbuf_head);
    }
}

static void amdvi_cmdbuf_run(AMDVIState *s)
{
    if (!s->cmdbuf_enabled) {
        trace_amdvi_command_error(amdvi_readq(s, AMDVI_MMIO_CONTROL));
        return;
    }

    /* check if there is work to do. */
    while (s->cmdbuf_head != s->cmdbuf_tail) {
        trace_amdvi_command_exec(s->cmdbuf_head, s->cmdbuf_tail, s->cmdbuf);
        amdvi_cmdbuf_exec(s);
        s->cmdbuf_head += AMDVI_COMMAND_SIZE;
        amdvi_writeq_raw(s, s->cmdbuf_head, AMDVI_MMIO_COMMAND_HEAD);

        /* wrap head pointer */
        if (s->cmdbuf_head >= s->cmdbuf_len * AMDVI_COMMAND_SIZE) {
            s->cmdbuf_head = 0;
        }
    }
}

static void amdvi_mmio_trace(hwaddr addr, unsigned size)
{
    uint8_t index = (addr & ~0x2000) / 8;

    if ((addr & 0x2000)) {
        /* high table */
        index = index >= AMDVI_MMIO_REGS_HIGH ? AMDVI_MMIO_REGS_HIGH : index;
        trace_amdvi_mmio_read(amdvi_mmio_high[index], addr, size, addr & ~0x07);
    } else {
        index = index >= AMDVI_MMIO_REGS_LOW ? AMDVI_MMIO_REGS_LOW : index;
        trace_amdvi_mmio_read(amdvi_mmio_high[index], addr, size, addr & ~0x07);
    }
}

static uint64_t amdvi_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    AMDVIState *s = opaque;

    uint64_t val = -1;
    if (addr + size > AMDVI_MMIO_SIZE) {
        trace_amdvi_mmio_read("error: addr outside region: max ",
                (uint64_t)AMDVI_MMIO_SIZE, addr, size);
        return (uint64_t)-1;
    }

    if (size == 2) {
        val = amdvi_readw(s, addr);
    } else if (size == 4) {
        val = amdvi_readl(s, addr);
    } else if (size == 8) {
        val = amdvi_readq(s, addr);
    }
    amdvi_mmio_trace(addr, size);

    return val;
}

static void amdvi_handle_control_write(AMDVIState *s)
{
    unsigned long control = amdvi_readq(s, AMDVI_MMIO_CONTROL);
    s->enabled = !!(control & AMDVI_MMIO_CONTROL_AMDVIEN);

    s->ats_enabled = !!(control & AMDVI_MMIO_CONTROL_HTTUNEN);
    s->evtlog_enabled = s->enabled && !!(control &
                        AMDVI_MMIO_CONTROL_EVENTLOGEN);

    s->evtlog_intr = !!(control & AMDVI_MMIO_CONTROL_EVENTINTEN);
    s->completion_wait_intr = !!(control & AMDVI_MMIO_CONTROL_COMWAITINTEN);
    s->cmdbuf_enabled = s->enabled && !!(control &
                        AMDVI_MMIO_CONTROL_CMDBUFLEN);

    /* update the flags depending on the control register */
    if (s->cmdbuf_enabled) {
        amdvi_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_CMDBUF_RUN);
    } else {
        amdvi_assign_andq(s, AMDVI_MMIO_STATUS, ~AMDVI_MMIO_STATUS_CMDBUF_RUN);
    }
    if (s->evtlog_enabled) {
        amdvi_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_EVT_RUN);
    } else {
        amdvi_assign_andq(s, AMDVI_MMIO_STATUS, ~AMDVI_MMIO_STATUS_EVT_RUN);
    }

    trace_amdvi_control_status(control);
    amdvi_cmdbuf_run(s);
}

static inline void amdvi_handle_devtab_write(AMDVIState *s)

{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_DEVICE_TABLE);
    s->devtab = (val & AMDVI_MMIO_DEVTAB_BASE_MASK);

    /* set device table length */
    s->devtab_len = ((val & AMDVI_MMIO_DEVTAB_SIZE_MASK) + 1 *
                    (AMDVI_MMIO_DEVTAB_SIZE_UNIT /
                     AMDVI_MMIO_DEVTAB_ENTRY_SIZE));
}

static inline void amdvi_handle_cmdhead_write(AMDVIState *s)
{
    s->cmdbuf_head = amdvi_readq(s, AMDVI_MMIO_COMMAND_HEAD)
                     & AMDVI_MMIO_CMDBUF_HEAD_MASK;
    amdvi_cmdbuf_run(s);
}

static inline void amdvi_handle_cmdbase_write(AMDVIState *s)
{
    s->cmdbuf = amdvi_readq(s, AMDVI_MMIO_COMMAND_BASE)
                & AMDVI_MMIO_CMDBUF_BASE_MASK;
    s->cmdbuf_len = 1UL << (amdvi_readq(s, AMDVI_MMIO_CMDBUF_SIZE_BYTE)
                    & AMDVI_MMIO_CMDBUF_SIZE_MASK);
    s->cmdbuf_head = s->cmdbuf_tail = 0;
}

static inline void amdvi_handle_cmdtail_write(AMDVIState *s)
{
    s->cmdbuf_tail = amdvi_readq(s, AMDVI_MMIO_COMMAND_TAIL)
                     & AMDVI_MMIO_CMDBUF_TAIL_MASK;
    amdvi_cmdbuf_run(s);
}

static inline void amdvi_handle_excllim_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_EXCL_LIMIT);
    s->excl_limit = (val & AMDVI_MMIO_EXCL_LIMIT_MASK) |
                    AMDVI_MMIO_EXCL_LIMIT_LOW;
}

static inline void amdvi_handle_evtbase_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_EVENT_BASE);
    s->evtlog = val & AMDVI_MMIO_EVTLOG_BASE_MASK;
    s->evtlog_len = 1UL << (amdvi_readq(s, AMDVI_MMIO_EVTLOG_SIZE_BYTE)
                    & AMDVI_MMIO_EVTLOG_SIZE_MASK);
}

static inline void amdvi_handle_evttail_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_EVENT_TAIL);
    s->evtlog_tail = val & AMDVI_MMIO_EVTLOG_TAIL_MASK;
}

static inline void amdvi_handle_evthead_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_EVENT_HEAD);
    s->evtlog_head = val & AMDVI_MMIO_EVTLOG_HEAD_MASK;
}

static inline void amdvi_handle_pprbase_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_PPR_BASE);
    s->ppr_log = val & AMDVI_MMIO_PPRLOG_BASE_MASK;
    s->pprlog_len = 1UL << (amdvi_readq(s, AMDVI_MMIO_PPRLOG_SIZE_BYTE)
                    & AMDVI_MMIO_PPRLOG_SIZE_MASK);
}

static inline void amdvi_handle_pprhead_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_PPR_HEAD);
    s->pprlog_head = val & AMDVI_MMIO_PPRLOG_HEAD_MASK;
}

static inline void amdvi_handle_pprtail_write(AMDVIState *s)
{
    uint64_t val = amdvi_readq(s, AMDVI_MMIO_PPR_TAIL);
    s->pprlog_tail = val & AMDVI_MMIO_PPRLOG_TAIL_MASK;
}

/* FIXME: something might go wrong if System Software writes in chunks
 * of one byte but linux writes in chunks of 4 bytes so currently it
 * works correctly with linux but will definitely be busted if software
 * reads/writes 8 bytes
 */
static void amdvi_mmio_reg_write(AMDVIState *s, unsigned size, uint64_t val,
                                 hwaddr addr)
{
    if (size == 2) {
        amdvi_writew(s, addr, val);
    } else if (size == 4) {
        amdvi_writel(s, addr, val);
    } else if (size == 8) {
        amdvi_writeq(s, addr, val);
    }
}

static void amdvi_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    AMDVIState *s = opaque;
    unsigned long offset = addr & 0x07;

    if (addr + size > AMDVI_MMIO_SIZE) {
        trace_amdvi_mmio_write("error: addr outside region: max ",
                (uint64_t)AMDVI_MMIO_SIZE, size, val, offset);
        return;
    }

    amdvi_mmio_trace(addr, size);
    switch (addr & ~0x07) {
    case AMDVI_MMIO_CONTROL:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_control_write(s);
        break;
    case AMDVI_MMIO_DEVICE_TABLE:
        amdvi_mmio_reg_write(s, size, val, addr);
       /*  set device table address
        *   This also suffers from inability to tell whether software
        *   is done writing
        */
        if (offset || (size == 8)) {
            amdvi_handle_devtab_write(s);
        }
        break;
    case AMDVI_MMIO_COMMAND_HEAD:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_cmdhead_write(s);
        break;
    case AMDVI_MMIO_COMMAND_BASE:
        amdvi_mmio_reg_write(s, size, val, addr);
        /* FIXME - make sure System Software has finished writing incase
         * it writes in chucks less than 8 bytes in a robust way.As for
         * now, this hacks works for the linux driver
         */
        if (offset || (size == 8)) {
            amdvi_handle_cmdbase_write(s);
        }
        break;
    case AMDVI_MMIO_COMMAND_TAIL:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_cmdtail_write(s);
        break;
    case AMDVI_MMIO_EVENT_BASE:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_evtbase_write(s);
        break;
    case AMDVI_MMIO_EVENT_HEAD:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_evthead_write(s);
        break;
    case AMDVI_MMIO_EVENT_TAIL:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_evttail_write(s);
        break;
    case AMDVI_MMIO_EXCL_LIMIT:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_excllim_write(s);
        break;
        /* PPR log base - unused for now */
    case AMDVI_MMIO_PPR_BASE:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_pprbase_write(s);
        break;
        /* PPR log head - also unused for now */
    case AMDVI_MMIO_PPR_HEAD:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_pprhead_write(s);
        break;
        /* PPR log tail - unused for now */
    case AMDVI_MMIO_PPR_TAIL:
        amdvi_mmio_reg_write(s, size, val, addr);
        amdvi_handle_pprtail_write(s);
        break;
    }
}

static inline uint64_t amdvi_get_perms(uint64_t entry)
{
    return (entry & (AMDVI_DEV_PERM_READ | AMDVI_DEV_PERM_WRITE)) >>
           AMDVI_DEV_PERM_SHIFT;
}

/* a valid entry should have V = 1 and reserved bits honoured */
static bool amdvi_validate_dte(AMDVIState *s, uint16_t devid,
                               uint64_t *dte)
{
    if ((dte[0] & AMDVI_DTE_LOWER_QUAD_RESERVED)
        || (dte[1] & AMDVI_DTE_MIDDLE_QUAD_RESERVED)
        || (dte[2] & AMDVI_DTE_UPPER_QUAD_RESERVED) || dte[3]) {
        amdvi_log_illegaldevtab_error(s, devid,
                                      s->devtab +
                                      devid * AMDVI_DEVTAB_ENTRY_SIZE, 0);
        return false;
    }

    return dte[0] & AMDVI_DEV_VALID;
}

/* get a device table entry given the devid */
static bool amdvi_get_dte(AMDVIState *s, int devid, uint64_t *entry)
{
    uint32_t offset = devid * AMDVI_DEVTAB_ENTRY_SIZE;

    if (dma_memory_read(&address_space_memory, s->devtab + offset, entry,
        AMDVI_DEVTAB_ENTRY_SIZE)) {
        trace_amdvi_dte_get_fail(s->devtab, offset);
        /* log error accessing dte */
        amdvi_log_devtab_error(s, devid, s->devtab + offset, 0);
        return false;
    }

    *entry = le64_to_cpu(*entry);
    if (!amdvi_validate_dte(s, devid, entry)) {
        trace_amdvi_invalid_dte(entry[0]);
        return false;
    }

    return true;
}

/* get pte translation mode */
static inline uint8_t get_pte_translation_mode(uint64_t pte)
{
    return (pte >> AMDVI_DEV_MODE_RSHIFT) & AMDVI_DEV_MODE_MASK;
}

static inline uint64_t pte_override_page_mask(uint64_t pte)
{
    uint8_t page_mask = 12;
    uint64_t addr = (pte & AMDVI_DEV_PT_ROOT_MASK) ^ AMDVI_DEV_PT_ROOT_MASK;
    /* find the first zero bit */
    while (addr & 1) {
        page_mask++;
        addr = addr >> 1;
    }

    return ~((1ULL << page_mask) - 1);
}

static inline uint64_t pte_get_page_mask(uint64_t oldlevel)
{
    return ~((1UL << ((oldlevel * 9) + 3)) - 1);
}

static inline uint64_t amdvi_get_pte_entry(AMDVIState *s, uint64_t pte_addr,
                                          uint16_t devid)
{
    uint64_t pte;

    if (dma_memory_read(&address_space_memory, pte_addr, &pte, sizeof(pte))) {
        trace_amdvi_get_pte_hwerror(pte_addr);
        amdvi_log_pagetab_error(s, devid, pte_addr, 0);
        pte = 0;
        return pte;
    }

    pte = le64_to_cpu(pte);
    return pte;
}

static void amdvi_page_walk(AMDVIAddressSpace *as, uint64_t *dte,
                            IOMMUTLBEntry *ret, unsigned perms,
                            hwaddr addr)
{
    unsigned level, present, pte_perms, oldlevel;
    uint64_t pte = dte[0], pte_addr, page_mask;

    /* make sure the DTE has TV = 1 */
    if (pte & AMDVI_DEV_TRANSLATION_VALID) {
        level = get_pte_translation_mode(pte);
        if (level >= 7) {
            trace_amdvi_mode_invalid(level, addr);
            return;
        }
        if (level == 0) {
            goto no_remap;
        }

        /* we are at the leaf page table or page table encodes a huge page */
        while (level > 0) {
            pte_perms = amdvi_get_perms(pte);
            present = pte & 1;
            if (!present || perms != (perms & pte_perms)) {
                amdvi_page_fault(as->iommu_state, as->devfn, addr, perms);
                trace_amdvi_page_fault(addr);
                return;
            }

            /* go to the next lower level */
            pte_addr = pte & AMDVI_DEV_PT_ROOT_MASK;
            /* add offset and load pte */
            pte_addr += ((addr >> (3 + 9 * level)) & 0x1FF) << 3;
            pte = amdvi_get_pte_entry(as->iommu_state, pte_addr, as->devfn);
            if (!pte) {
                return;
            }
            oldlevel = level;
            level = get_pte_translation_mode(pte);
            if (level == 0x7) {
                break;
            }
        }

        if (level == 0x7) {
            page_mask = pte_override_page_mask(pte);
        } else {
            page_mask = pte_get_page_mask(oldlevel);
        }

        /* get access permissions from pte */
        ret->iova = addr & page_mask;
        ret->translated_addr = (pte & AMDVI_DEV_PT_ROOT_MASK) & page_mask;
        ret->addr_mask = ~page_mask;
        ret->perm = amdvi_get_perms(pte);
        return;
    }
no_remap:
    ret->iova = addr & AMDVI_PAGE_MASK_4K;
    ret->translated_addr = addr & AMDVI_PAGE_MASK_4K;
    ret->addr_mask = ~AMDVI_PAGE_MASK_4K;
    ret->perm = amdvi_get_perms(pte);
}

static void amdvi_do_translate(AMDVIAddressSpace *as, hwaddr addr,
                               bool is_write, IOMMUTLBEntry *ret)
{
    AMDVIState *s = as->iommu_state;
    uint16_t devid = PCI_BUILD_BDF(as->bus_num, as->devfn);
    AMDVIIOTLBEntry *iotlb_entry = amdvi_iotlb_lookup(s, addr, devid);
    uint64_t entry[4];

    if (iotlb_entry) {
        trace_amdvi_iotlb_hit(PCI_BUS_NUM(devid), PCI_SLOT(devid),
                PCI_FUNC(devid), addr, iotlb_entry->translated_addr);
        ret->iova = addr & ~iotlb_entry->page_mask;
        ret->translated_addr = iotlb_entry->translated_addr;
        ret->addr_mask = iotlb_entry->page_mask;
        ret->perm = iotlb_entry->perms;
        return;
    }

    /* devices with V = 0 are not translated */
    if (!amdvi_get_dte(s, devid, entry)) {
        goto out;
    }

    amdvi_page_walk(as, entry, ret,
                    is_write ? AMDVI_PERM_WRITE : AMDVI_PERM_READ, addr);

    amdvi_update_iotlb(s, devid, addr, *ret,
                       entry[1] & AMDVI_DEV_DOMID_ID_MASK);
    return;

out:
    ret->iova = addr & AMDVI_PAGE_MASK_4K;
    ret->translated_addr = addr & AMDVI_PAGE_MASK_4K;
    ret->addr_mask = ~AMDVI_PAGE_MASK_4K;
    ret->perm = IOMMU_RW;
}

static inline bool amdvi_is_interrupt_addr(hwaddr addr)
{
    return addr >= AMDVI_INT_ADDR_FIRST && addr <= AMDVI_INT_ADDR_LAST;
}

static IOMMUTLBEntry amdvi_translate(MemoryRegion *iommu, hwaddr addr,
                                     bool is_write)
{
    AMDVIAddressSpace *as = container_of(iommu, AMDVIAddressSpace, iommu);
    AMDVIState *s = as->iommu_state;
    IOMMUTLBEntry ret = {
        .target_as = &address_space_memory,
        .iova = addr,
        .translated_addr = 0,
        .addr_mask = ~(hwaddr)0,
        .perm = IOMMU_NONE
    };

    if (!s->enabled) {
        /* AMDVI disabled - corresponds to iommu=off not
         * failure to provide any parameter
         */
        ret.iova = addr & AMDVI_PAGE_MASK_4K;
        ret.translated_addr = addr & AMDVI_PAGE_MASK_4K;
        ret.addr_mask = ~AMDVI_PAGE_MASK_4K;
        ret.perm = IOMMU_RW;
        return ret;
    } else if (amdvi_is_interrupt_addr(addr)) {
        ret.iova = addr & AMDVI_PAGE_MASK_4K;
        ret.translated_addr = addr & AMDVI_PAGE_MASK_4K;
        ret.addr_mask = ~AMDVI_PAGE_MASK_4K;
        ret.perm = IOMMU_WO;
        return ret;
    }

    amdvi_do_translate(as, addr, is_write, &ret);
    trace_amdvi_translation_result(as->bus_num, PCI_SLOT(as->devfn),
            PCI_FUNC(as->devfn), addr, ret.translated_addr);
    return ret;
}

static AddressSpace *amdvi_host_dma_iommu(PCIBus *bus, void *opaque, int devfn)
{
    AMDVIState *s = opaque;
    AMDVIAddressSpace **iommu_as;
    int bus_num = pci_bus_num(bus);

    iommu_as = s->address_spaces[bus_num];

    /* allocate memory during the first run */
    if (!iommu_as) {
        iommu_as = g_malloc0(sizeof(AMDVIAddressSpace *) * PCI_DEVFN_MAX);
        s->address_spaces[bus_num] = iommu_as;
    }

    /* set up AMD-Vi region */
    if (!iommu_as[devfn]) {
        iommu_as[devfn] = g_malloc0(sizeof(AMDVIAddressSpace));
        iommu_as[devfn]->bus_num = (uint8_t)bus_num;
        iommu_as[devfn]->devfn = (uint8_t)devfn;
        iommu_as[devfn]->iommu_state = s;

        memory_region_init_iommu(&iommu_as[devfn]->iommu, OBJECT(s),
                                 &s->iommu_ops, "amd-iommu", UINT64_MAX);
        address_space_init(&iommu_as[devfn]->as, &iommu_as[devfn]->iommu,
                           "amd-iommu");
    }
    return &iommu_as[devfn]->as;
}

static const MemoryRegionOps mmio_mem_ops = {
    .read = amdvi_mmio_read,
    .write = amdvi_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = false,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    }
};

static void amdvi_iommu_notify_started(MemoryRegion *iommu)
{
    AMDVIAddressSpace *as = container_of(iommu, AMDVIAddressSpace, iommu);

    hw_error("device %02x.%02x.%x requires iommu notifier which is not "
             "currently supported", as->bus_num, PCI_SLOT(as->devfn),
             PCI_FUNC(as->devfn));
}

static void amdvi_init(AMDVIState *s)
{
    amdvi_iotlb_reset(s);

    s->iommu_ops.translate = amdvi_translate;
    s->iommu_ops.notify_started = amdvi_iommu_notify_started;
    s->devtab_len = 0;
    s->cmdbuf_len = 0;
    s->cmdbuf_head = 0;
    s->cmdbuf_tail = 0;
    s->evtlog_head = 0;
    s->evtlog_tail = 0;
    s->excl_enabled = false;
    s->excl_allow = false;
    s->mmio_enabled = false;
    s->enabled = false;
    s->ats_enabled = false;
    s->cmdbuf_enabled = false;

    /* reset MMIO */
    memset(s->mmior, 0, AMDVI_MMIO_SIZE);
    amdvi_set_quad(s, AMDVI_MMIO_EXT_FEATURES, AMDVI_EXT_FEATURES,
            0xffffffffffffffef, 0);
    amdvi_set_quad(s, AMDVI_MMIO_STATUS, 0, 0x98, 0x67);

    /* reset device ident */
    pci_config_set_vendor_id(s->pci.dev.config, PCI_VENDOR_ID_AMD);
    pci_config_set_prog_interface(s->pci.dev.config, 00);
    pci_config_set_device_id(s->pci.dev.config, s->devid);
    pci_config_set_class(s->pci.dev.config, 0x0806);

    /* reset AMDVI specific capabilities, all r/o */
    pci_set_long(s->pci.dev.config + s->capab_offset, AMDVI_CAPAB_FEATURES);
    pci_set_long(s->pci.dev.config + s->capab_offset + AMDVI_CAPAB_BAR_LOW,
                 s->mmio.addr & ~(0xffff0000));
    pci_set_long(s->pci.dev.config + s->capab_offset + AMDVI_CAPAB_BAR_HIGH,
                (s->mmio.addr & ~(0xffff)) >> 16);
    pci_set_long(s->pci.dev.config + s->capab_offset + AMDVI_CAPAB_RANGE,
                 0xff000000);
    pci_set_long(s->pci.dev.config + s->capab_offset + AMDVI_CAPAB_MISC, 0);
    pci_set_long(s->pci.dev.config + s->capab_offset + AMDVI_CAPAB_MISC,
            AMDVI_MAX_PH_ADDR | AMDVI_MAX_GVA_ADDR | AMDVI_MAX_VA_ADDR);
}

static void amdvi_reset(DeviceState *dev)
{
    AMDVIState *s = AMD_IOMMU_DEVICE(dev);

    msi_reset(&s->pci.dev);
    amdvi_init(s);
}

static void amdvi_realize(DeviceState *dev, Error **err)
{
    AMDVIState *s = AMD_IOMMU_DEVICE(dev);
    X86IOMMUState *x86_iommu = X86_IOMMU_DEVICE(dev);
    PCIBus *bus = PC_MACHINE(qdev_get_machine())->bus;
    s->iotlb = g_hash_table_new_full(amdvi_uint64_hash,
                                     amdvi_uint64_equal, g_free, g_free);

    /* This device should take care of IOMMU PCI properties */
    x86_iommu->type = TYPE_AMD;
    qdev_set_parent_bus(DEVICE(&s->pci), &bus->qbus);
    object_property_set_bool(OBJECT(&s->pci), true, "realized", err);
    s->capab_offset = pci_add_capability(&s->pci.dev, AMDVI_CAPAB_ID_SEC, 0,
                                         AMDVI_CAPAB_SIZE);
    pci_add_capability(&s->pci.dev, PCI_CAP_ID_MSI, 0, AMDVI_CAPAB_REG_SIZE);
    pci_add_capability(&s->pci.dev, PCI_CAP_ID_HT, 0, AMDVI_CAPAB_REG_SIZE);

    /* set up MMIO */
    memory_region_init_io(&s->mmio, OBJECT(s), &mmio_mem_ops, s, "amdvi-mmio",
                          AMDVI_MMIO_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
    sysbus_mmio_map(SYS_BUS_DEVICE(s), 0, AMDVI_BASE_ADDR);
    pci_setup_iommu(bus, amdvi_host_dma_iommu, s);
    s->devid = object_property_get_int(OBJECT(&s->pci), "addr", err);
    msi_init(&s->pci.dev, 0, 1, true, false, err);
    amdvi_init(s);
}

static const VMStateDescription vmstate_amdvi = {
    .name = "amd-iommu",
    .unmigratable = 1
};

static void amdvi_instance_init(Object *klass)
{
    AMDVIState *s = AMD_IOMMU_DEVICE(klass);

    object_initialize(&s->pci, sizeof(s->pci), TYPE_AMD_IOMMU_PCI);
}

static void amdvi_class_init(ObjectClass *klass, void* data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    X86IOMMUClass *dc_class = X86_IOMMU_CLASS(klass);

    dc->reset = amdvi_reset;
    dc->vmsd = &vmstate_amdvi;
    dc->hotpluggable = false;
    dc_class->realize = amdvi_realize;
}

static const TypeInfo amdvi = {
    .name = TYPE_AMD_IOMMU_DEVICE,
    .parent = TYPE_X86_IOMMU_DEVICE,
    .instance_size = sizeof(AMDVIState),
    .instance_init = amdvi_instance_init,
    .class_init = amdvi_class_init
};

static const TypeInfo amdviPCI = {
    .name = "AMDVI-PCI",
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AMDVIPCIState),
};

static void amdviPCI_register_types(void)
{
    type_register_static(&amdviPCI);
    type_register_static(&amdvi);
}

type_init(amdviPCI_register_types);
