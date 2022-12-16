/*
 * Xen HVM emulation support in KVM
 *
 * Copyright © 2019 Oracle and/or its affiliates. All rights reserved.
 * Copyright © 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_SYSEMU_KVM_XEN_H
#define QEMU_SYSEMU_KVM_XEN_H

void *kvm_xen_get_vcpu_info_hva(uint32_t vcpu_id);
void kvm_xen_inject_vcpu_callback_vector(uint32_t vcpu_id);

#endif /* QEMU_SYSEMU_KVM_XEN_H */
