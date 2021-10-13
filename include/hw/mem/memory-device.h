/*
 * Memory Device Interface
 *
 * Copyright (c) 2018 Red Hat, Inc.
 *
 * Authors:
 *  David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef MEMORY_DEVICE_H
#define MEMORY_DEVICE_H

#include "hw/qdev-core.h"
#include "qapi/qapi-types-machine.h"
#include "qom/object.h"

#define TYPE_MEMORY_DEVICE "memory-device"

typedef struct MemoryDeviceClass MemoryDeviceClass;
DECLARE_CLASS_CHECKERS(MemoryDeviceClass, MEMORY_DEVICE,
                       TYPE_MEMORY_DEVICE)
#define MEMORY_DEVICE(obj) \
     INTERFACE_CHECK(MemoryDeviceState, (obj), TYPE_MEMORY_DEVICE)

typedef struct MemoryDeviceState MemoryDeviceState;

/**
 * MemoryDeviceClass:
 *
 * All memory devices need to implement TYPE_MEMORY_DEVICE as an interface.
 *
 * A memory device is a device that owns a memory region which is
 * mapped into guest physical address space at a certain address. The
 * address in guest physical memory can either be specified explicitly
 * or get assigned automatically.
 *
 * Conceptually, memory devices only span one memory region. If multiple
 * successive memory regions are used, a covering memory region has to
 * be provided. Scattered memory regions are not supported for single
 * devices.
 */
struct MemoryDeviceClass {
    /* private */
    InterfaceClass parent_class;

    /*
     * Return the address of the memory device in guest physical memory.
     *
     * Called when (un)plugging a memory device or when iterating over
     * all memory devices mapped into guest physical address space.
     *
     * If "0" is returned, no address has been specified by the user and
     * no address has been assigned to this memory device yet.
     */
    uint64_t (*get_addr)(const MemoryDeviceState *md);

    /*
     * Set the address of the memory device in guest physical memory.
     *
     * Called when plugging the memory device to configure the determined
     * address in guest physical memory.
     */
    void (*set_addr)(MemoryDeviceState *md, uint64_t addr, Error **errp);

    /*
     * Return the amount of memory provided by the memory device currently
     * usable ("plugged") by the VM.
     *
     * Called when calculating the total amount of ram available to the
     * VM (e.g. to report memory stats to the user).
     *
     * This is helpful for devices that dynamically manage the amount of
     * memory accessible by the guest via the reserved memory region. For
     * most devices, this corresponds to the size of the memory region.
     */
    uint64_t (*get_plugged_size)(const MemoryDeviceState *md, Error **errp);

    /*
     * Return the memory region of the memory device.
     *
     * Called when (un)plugging the memory device, to (un)map the
     * memory region in guest physical memory, but also to detect the
     * required alignment during address assignment or when the size of the
     * memory region is required.
     */
    MemoryRegion *(*get_memory_region)(MemoryDeviceState *md, Error **errp);

    /*
     * Optional: Return the desired minimum alignment of the device in guest
     * physical address space. The final alignment is computed based on this
     * alignment and the alignment requirements of the memory region.
     *
     * Called when plugging the memory device to detect the required alignment
     * during address assignment.
     */
    uint64_t (*get_min_alignment)(const MemoryDeviceState *md);

    /*
     * Optional: Return the number of used individual memslots (i.e.,
     * individual RAM mappings) the device has created in the memory region of
     * the device. The device has to make sure that memslots won't get merged
     * internally (e.g,, by disabling merging of memory region aliases) if the
     * memory region layout could allow for that.
     *
     * If this function is not implemented, we assume the device memory region
     * is not a container and that there is exactly one memslot.
     *
     * Called when plugging the memory device or when iterating over
     * all realized memory devices to calculate used/reserved/available
     * memslots.
     */
    unsigned int (*get_used_memslots)(const MemoryDeviceState *md, Error **errp);

    /*
     * Optional: Return the total number of individual memslots
     * (i.e., individual RAM mappings) the device may create in the the memory
     * region of the device over its lifetime. The result must never change.
     *
     * If this function is not implemented, we assume the device memory region
     * is not a container and that there will be exactly one memslot.
     *
     * Called when plugging the memory device or when iterating over
     * all realized memory devices to calculate used/reserved/available
     * memslots.
     */
    unsigned int (*get_memslots)(const MemoryDeviceState *md, Error **errp);

    /*
     * Translate the memory device into #MemoryDeviceInfo.
     */
    void (*fill_device_info)(const MemoryDeviceState *md,
                             MemoryDeviceInfo *info);
};

MemoryDeviceInfoList *qmp_memory_device_list(void);
uint64_t get_plugged_memory_size(void);
void memory_device_pre_plug(MemoryDeviceState *md, MachineState *ms,
                            const uint64_t *legacy_align, Error **errp);
void memory_device_plug(MemoryDeviceState *md, MachineState *ms);
void memory_device_unplug(MemoryDeviceState *md, MachineState *ms);
uint64_t memory_device_get_region_size(const MemoryDeviceState *md,
                                       Error **errp);
unsigned int memory_devices_get_reserved_memslots(void);
unsigned int memory_devices_calc_memslot_limit(uint64_t region_size);

#endif
