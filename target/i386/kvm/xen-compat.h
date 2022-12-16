/*
 * Xen HVM emulation support in KVM
 *
 * Copyright © 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_I386_KVM_XEN_COMPAT_H
#define QEMU_I386_KVM_XEN_COMPAT_H

#include "standard-headers/xen/memory.h"

typedef uint32_t compat_pfn_t;
typedef uint32_t compat_ulong_t;
typedef uint32_t compat_ptr_t;

#define __DEFINE_COMPAT_HANDLE(name, type)      \
    typedef struct {                            \
        compat_ptr_t c;                         \
        type *_[0] __attribute__((packed));     \
    } __compat_handle_ ## name;                 \

#define DEFINE_COMPAT_HANDLE(name) __DEFINE_COMPAT_HANDLE(name, name)
#define COMPAT_HANDLE(name) __compat_handle_ ## name

DEFINE_COMPAT_HANDLE(compat_pfn_t);
DEFINE_COMPAT_HANDLE(compat_ulong_t);
DEFINE_COMPAT_HANDLE(int);

struct compat_xen_add_to_physmap {
    domid_t domid;
    uint16_t size;
    unsigned int space;
    compat_ulong_t idx;
    compat_pfn_t gpfn;
};

struct compat_xen_add_to_physmap_batch {
    domid_t domid;
    uint16_t space;
    uint16_t size;
    uint16_t extra;
    COMPAT_HANDLE(compat_ulong_t) idxs;
    COMPAT_HANDLE(compat_pfn_t) gpfns;
    COMPAT_HANDLE(int) errs;
};

#endif /* QEMU_I386_XEN_COMPAT_H */
