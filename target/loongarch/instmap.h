/*
 * LoongArch emulation for qemu: instruction opcode
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef TARGET_LOONGARCH_INSTMAP_H
#define TARGET_LOONGARCH_INSTMAP_H

/* fixed point opcodes */
enum {
    LA_OPC_ADD_W     = (0x00020 << 15),
    LA_OPC_ADD_D     = (0x00021 << 15),
    LA_OPC_SUB_W     = (0x00022 << 15),
    LA_OPC_SUB_D     = (0x00023 << 15),
    LA_OPC_SLT       = (0x00024 << 15),
    LA_OPC_SLTU      = (0x00025 << 15),
    LA_OPC_NOR       = (0x00028 << 15),
    LA_OPC_AND       = (0x00029 << 15),
    LA_OPC_OR        = (0x0002A << 15),
    LA_OPC_XOR       = (0x0002B << 15),
    LA_OPC_MUL_W     = (0x00038 << 15),
    LA_OPC_MULH_W    = (0x00039 << 15),
    LA_OPC_MULH_WU   = (0x0003A << 15),
    LA_OPC_MUL_D     = (0x0003B << 15),
    LA_OPC_MULH_D    = (0x0003C << 15),
    LA_OPC_MULH_DU   = (0x0003D << 15),
    LA_OPC_DIV_W     = (0x00040 << 15),
    LA_OPC_MOD_W     = (0x00041 << 15),
    LA_OPC_DIV_WU    = (0x00042 << 15),
    LA_OPC_MOD_WU    = (0x00043 << 15),
    LA_OPC_DIV_D     = (0x00044 << 15),
    LA_OPC_MOD_D     = (0x00045 << 15),
    LA_OPC_DIV_DU    = (0x00046 << 15),
    LA_OPC_MOD_DU    = (0x00047 << 15),

    LA_OPC_ALSL_W    = (0x0002 << 17),
    LA_OPC_ALSL_D    = (0x0016 << 17)

};

/* 12 bit immediate opcodes */
enum {
    LA_OPC_SLTI      = (0x008 << 22),
    LA_OPC_SLTIU     = (0x009 << 22),
    LA_OPC_ADDI_W    = (0x00A << 22),
    LA_OPC_ADDI_D    = (0x00B << 22),
    LA_OPC_ANDI      = (0x00D << 22),
    LA_OPC_ORI       = (0x00E << 22),
    LA_OPC_XORI      = (0x00F << 22)
};
