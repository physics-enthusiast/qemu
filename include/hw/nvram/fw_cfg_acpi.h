#ifndef FW_CFG_ACPI_H
#define FW_CFG_ACPI_H

#include "qemu/osdep.h"
#include "exec/hwaddr.h"

void fw_cfg_acpi_dsdt_add(Aml *scope, const MemMapEntry *fw_cfg_memmap);

#endif
