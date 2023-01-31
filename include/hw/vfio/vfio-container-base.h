/*
 * VFIO BASE CONTAINER
 *
 * Copyright (C) 2022 Intel Corporation.
 * Copyright Red Hat, Inc. 2022
 *
 * Authors: Yi Liu <yi.l.liu@intel.com>
 *          Eric Auger <eric.auger@redhat.com>
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
 */

#ifndef HW_VFIO_VFIO_BASE_CONTAINER_H
#define HW_VFIO_VFIO_BASE_CONTAINER_H

#include "exec/memory.h"
#ifndef CONFIG_USER_ONLY
#include "exec/hwaddr.h"
#endif

typedef enum VFIOContainerFeature {
    VFIO_FEAT_LIVE_MIGRATION,
    VFIO_FEAT_DMA_COPY,
} VFIOContainerFeature;

typedef struct VFIOContainer VFIOContainer;

typedef struct VFIOAddressSpace {
    AddressSpace *as;
    MemoryListener listener;
    bool listener_initialized;
    QLIST_HEAD(, VFIOContainer) containers;
    QLIST_ENTRY(VFIOAddressSpace) list;
} VFIOAddressSpace;

typedef struct VFIOGuestIOMMU {
    VFIOContainer *container;
    IOMMUMemoryRegion *iommu_mr;
    hwaddr iommu_offset;
    IOMMUNotifier n;
    QLIST_ENTRY(VFIOGuestIOMMU) giommu_next;
} VFIOGuestIOMMU;

typedef struct VFIORamDiscardListener {
    VFIOContainer *container;
    MemoryRegion *mr;
    hwaddr offset_within_address_space;
    hwaddr size;
    uint64_t granularity;
    RamDiscardListener listener;
    QLIST_ENTRY(VFIORamDiscardListener) next;
} VFIORamDiscardListener;

typedef struct VFIOHostDMAWindow {
    hwaddr min_iova;
    hwaddr max_iova;
    uint64_t iova_pgsizes;
    QLIST_ENTRY(VFIOHostDMAWindow) hostwin_next;
} VFIOHostDMAWindow;

typedef struct VFIODevice VFIODevice;
typedef struct VFIOIOMMUBackendOpsClass VFIOIOMMUBackendOpsClass;

/*
 * This is the base object for vfio container backends
 */
struct VFIOContainer {
    VFIOIOMMUBackendOpsClass *ops;
    VFIOAddressSpace *space;
    Error *error;
    bool initialized;
    bool dirty_pages_supported;
    uint64_t dirty_pgsizes;
    uint64_t max_dirty_bitmap_size;
    unsigned long pgsizes;
    unsigned int dma_max_mappings;
    QLIST_HEAD(, VFIOGuestIOMMU) giommu_list;
    QLIST_HEAD(, VFIOHostDMAWindow) hostwin_list;
    QLIST_HEAD(, VFIORamDiscardListener) vrdl_list;
    QLIST_ENTRY(VFIOContainer) next;
};

bool vfio_container_check_extension(VFIOContainer *container,
                                    VFIOContainerFeature feat);
int vfio_container_dma_map(VFIOContainer *container,
                           hwaddr iova, ram_addr_t size,
                           void *vaddr, bool readonly);
int vfio_container_dma_copy(VFIOContainer *src, VFIOContainer *dst,
                            hwaddr iova, ram_addr_t size, bool readonly);
int vfio_container_dma_unmap(VFIOContainer *container,
                             hwaddr iova, ram_addr_t size,
                             IOMMUTLBEntry *iotlb);
int vfio_container_reset(VFIOContainer *container);
bool vfio_container_devices_all_dirty_tracking(VFIOContainer *container);
void vfio_container_set_dirty_page_tracking(VFIOContainer *container,
                                            bool start);
int vfio_container_get_dirty_bitmap(VFIOContainer *container, uint64_t iova,
                                    uint64_t size, ram_addr_t ram_addr);
int vfio_container_add_section_window(VFIOContainer *container,
                                      MemoryRegionSection *section,
                                      Error **errp);
void vfio_container_del_section_window(VFIOContainer *container,
                                       MemoryRegionSection *section);

void vfio_container_init(VFIOContainer *container,
                         VFIOAddressSpace *space,
                         struct VFIOIOMMUBackendOpsClass *ops);
void vfio_container_destroy(VFIOContainer *container);

#define TYPE_VFIO_IOMMU_BACKEND_LEGACY_OPS "vfio-iommu-backend-legacy-ops"
#define TYPE_VFIO_IOMMU_BACKEND_IOMMUFD_OPS "vfio-iommu-backend-iommufd-ops"
#define TYPE_VFIO_IOMMU_BACKEND_OPS "vfio-iommu-backend-ops"

DECLARE_CLASS_CHECKERS(VFIOIOMMUBackendOpsClass,
                       VFIO_IOMMU_BACKEND_OPS, TYPE_VFIO_IOMMU_BACKEND_OPS)

struct VFIOIOMMUBackendOpsClass {
    /*< private >*/
    ObjectClass parent_class;

    /*< public >*/
    /* required */
    bool (*check_extension)(VFIOContainer *container,
                            VFIOContainerFeature feat);
    int (*dma_map)(VFIOContainer *container,
                   hwaddr iova, ram_addr_t size,
                   void *vaddr, bool readonly);
    int (*dma_copy)(VFIOContainer *src, VFIOContainer *dst,
                    hwaddr iova, ram_addr_t size, bool readonly);
    int (*dma_unmap)(VFIOContainer *container,
                     hwaddr iova, ram_addr_t size,
                     IOMMUTLBEntry *iotlb);
    int (*attach_device)(VFIODevice *vbasedev, AddressSpace *as, Error **errp);
    void (*detach_device)(VFIODevice *vbasedev);
    int (*reset)(VFIOContainer *container);
    /* migration feature */
    bool (*devices_all_dirty_tracking)(VFIOContainer *container);
    void (*set_dirty_page_tracking)(VFIOContainer *container, bool start);
    int (*get_dirty_bitmap)(VFIOContainer *container, uint64_t iova,
                            uint64_t size, ram_addr_t ram_addr);

    /* SPAPR specific */
    int (*add_window)(VFIOContainer *container,
                      MemoryRegionSection *section,
                      Error **errp);
    void (*del_window)(VFIOContainer *container,
                       MemoryRegionSection *section);
};


#endif /* HW_VFIO_VFIO_BASE_CONTAINER_H */
