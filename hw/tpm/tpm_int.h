/*
 * TPM configuration
 *
 * Copyright (C) 2011-2013 IBM Corporation
 *
 * Authors:
 *  Stefan Berger    <stefanb@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef TPM_TPM_INT_H
#define TPM_TPM_INT_H

#define TPM_STANDARD_CMDLINE_OPTS \
    { \
        .name = "type", \
        .type = QEMU_OPT_STRING, \
        .help = "Type of TPM backend", \
    }

struct tpm_req_hdr {
    uint16_t tag;
    uint32_t len;
    uint32_t ordinal;
} QEMU_PACKED;

struct tpm_resp_hdr {
    uint16_t tag;
    uint32_t len;
    uint32_t errcode;
} QEMU_PACKED;

#define TPM_TAG_RQU_COMMAND       0xc1
#define TPM_TAG_RQU_AUTH1_COMMAND 0xc2
#define TPM_TAG_RQU_AUTH2_COMMAND 0xc3

#define TPM_TAG_RSP_COMMAND       0xc4
#define TPM_TAG_RSP_AUTH1_COMMAND 0xc5
#define TPM_TAG_RSP_AUTH2_COMMAND 0xc6

#define TPM_FAIL                  9

#define TPM_ORD_ContinueSelfTest  0x53
#define TPM_ORD_GetTicks          0xf1
#define TPM_ORD_GetCapability     0x65

#define TPM_CAP_PROPERTY          0x05

#define TPM_CAP_PROP_INPUT_BUFFER 0x124

/* TPM2 defines */
#define TPM2_ST_NO_SESSIONS       0x8001

#define TPM2_CC_ReadClock         0x00000181
#define TPM2_CC_GetCapability     0x0000017a

#define TPM2_CAP_TPM_PROPERTIES   0x6

#define TPM2_PT_MAX_COMMAND_SIZE  0x11e

#endif /* TPM_TPM_INT_H */
