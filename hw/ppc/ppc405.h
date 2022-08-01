/*
 * QEMU PowerPC 405 shared definitions
 *
 * Copyright (c) 2007 Jocelyn Mayer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef PPC405_H
#define PPC405_H

#include "qom/object.h"
#include "hw/ppc/ppc4xx.h"

#define PPC405EP_SDRAM_BASE 0x00000000
#define PPC405EP_NVRAM_BASE 0xF0000000
#define PPC405EP_FPGA_BASE  0xF0300000
#define PPC405EP_SRAM_BASE  0xFFF00000
#define PPC405EP_SRAM_SIZE  (512 * KiB)
#define PPC405EP_FLASH_BASE 0xFFF80000

/* Bootinfo as set-up by u-boot */
typedef struct ppc4xx_bd_info_t ppc4xx_bd_info_t;
struct ppc4xx_bd_info_t {
    uint32_t bi_memstart;
    uint32_t bi_memsize;
    uint32_t bi_flashstart;
    uint32_t bi_flashsize;
    uint32_t bi_flashoffset; /* 0x10 */
    uint32_t bi_sramstart;
    uint32_t bi_sramsize;
    uint32_t bi_bootflags;
    uint32_t bi_ipaddr; /* 0x20 */
    uint8_t  bi_enetaddr[6];
    uint16_t bi_ethspeed;
    uint32_t bi_intfreq;
    uint32_t bi_busfreq; /* 0x30 */
    uint32_t bi_baudrate;
    uint8_t  bi_s_version[4];
    uint8_t  bi_r_version[32];
    uint32_t bi_procfreq;
    uint32_t bi_plb_busfreq;
    uint32_t bi_pci_busfreq;
    uint8_t  bi_pci_enetaddr[6];
    uint8_t  bi_pci_enetaddr2[6]; /* PPC405EP specific */
    uint32_t bi_opbfreq;
    uint32_t bi_iic_fast[2];
};

#define TYPE_PPC405_SOC "ppc405-soc"
OBJECT_DECLARE_SIMPLE_TYPE(Ppc405SoCState, PPC405_SOC);

struct Ppc405SoCState {
    /* Private */
    DeviceState parent_obj;

    /* Public */
    MemoryRegion sram;
    MemoryRegion ram_memories[2];
    hwaddr ram_bases[2], ram_sizes[2];
    bool do_dram_init;

    MemoryRegion *dram_mr;
    hwaddr ram_size;

    uint32_t sysclk;
    PowerPCCPU *cpu;
    DeviceState *uic;
};

/* PowerPC 405 core */
ram_addr_t ppc405_set_bootinfo(CPUPPCState *env, ram_addr_t ram_size);

void ppc4xx_plb_init(CPUPPCState *env);
void ppc405_ebc_init(CPUPPCState *env);

#endif /* PPC405_H */
