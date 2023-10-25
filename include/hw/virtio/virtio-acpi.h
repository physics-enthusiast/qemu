/*
 * virtio ACPI support
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VIRTIO_ACPI_H
#define VIRTIO_ACPI_H

#include "qemu/osdep.h"
#include "exec/hwaddr.h"

void virtio_acpi_dsdt_add(Aml *scope, const hwaddr virtio_mmio_base,
                          const hwaddr virtio_mmio_size, uint32_t mmio_irq,
                          long int start_index, int num);

#endif
