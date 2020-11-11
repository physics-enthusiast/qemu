/*
 * QEMU CXL Devices
 *
 * Copyright (c) 2020 Intel
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See the
 * COPYING file in the top-level directory.
 */

#ifndef CXL_DEVICE_H
#define CXL_DEVICE_H

#include "hw/register.h"

/*
 * The following is how a CXL device's MMIO space is laid out. The only
 * requirement from the spec is that the capabilities array and the capability
 * headers start at offset 0 and are contiguously packed. The headers themselves
 * provide offsets to the register fields. For this emulation, registers will
 * start at offset 0x80 (m == 0x80). No secondary mailbox is implemented which
 * means that n = m + sizeof(mailbox registers) + sizeof(device registers).
 *
 * This is roughly described in 8.2.8 Figure 138 of the CXL 2.0 spec.
 *
 * n + PAYLOAD_SIZE_MAX  +---------------------------------+
 *                       |                                 |
 *                  ^    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |         Command Payload         |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  n    +---------------------------------+
 *                  ^    |                                 |
 *                  |    |    Device Capability Registers  |
 *                  |    |    x, mailbox, y                |
 *                  |    |                                 |
 *                  m    +---------------------------------+
 *                  ^    |     Device Capability Header y  |
 *                  |    +---------------------------------+
 *                  |    | Device Capability Header Mailbox|
 *                  |    +------------- --------------------
 *                  |    |     Device Capability Header x  |
 *                  |    +---------------------------------+
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |      Device Cap Array[0..n]     |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  |    |                                 |
 *                  0    +---------------------------------+
 */

#define CXL_DEVICE_CAP_HDR1_OFFSET 0x10 /* Figure 138 */
#define CXL_DEVICE_CAP_REG_SIZE 0x10 /* 8.2.8.2 */

#define CXL_DEVICE_REGISTERS_OFFSET 0x80 /* Read comment above */
#define CXL_DEVICE_REGISTERS_LENGTH 0x8 /* 8.2.8.3.1 */

#define CXL_MAILBOX_REGISTERS_OFFSET \
    (CXL_DEVICE_REGISTERS_OFFSET + CXL_DEVICE_REGISTERS_LENGTH)
#define CXL_MAILBOX_REGISTERS_SIZE 0x20
#define CXL_MAILBOX_PAYLOAD_SHIFT 11
#define CXL_MAILBOX_MAX_PAYLOAD_SIZE (1 << CXL_MAILBOX_PAYLOAD_SHIFT)
#define CXL_MAILBOX_REGISTERS_LENGTH \
    (CXL_MAILBOX_REGISTERS_SIZE + CXL_MAILBOX_MAX_PAYLOAD_SIZE)

typedef struct cxl_device_state {
    /* Boss container and caps registers */
    MemoryRegion device_registers;

    MemoryRegion caps;
    MemoryRegion device;
    MemoryRegion mailbox;

    MemoryRegion *pmem;
    MemoryRegion *vmem;

    bool active;
    uint16_t command;
    uint16_t payload_size;
    union {
        uint8_t caps_reg_state[CXL_DEVICE_CAP_REG_SIZE * 4]; /* ARRAY + 3 CAPS */
        uint32_t caps_reg_state32[0];
    };
} CXLDeviceState;

/* Initialize the register block for a device */
void cxl_device_register_block_init(Object *obj, CXLDeviceState *dev);

/* Set up default values for the register block */
void cxl_device_register_init_common(CXLDeviceState *dev);

/* CXL 2.0 - 8.2.8.1 */
REG32(CXL_DEV_CAP_ARRAY, 0) /* 48b!?!?! */
    FIELD(CXL_DEV_CAP_ARRAY, CAP_ID, 0, 16)
    FIELD(CXL_DEV_CAP_ARRAY, CAP_VERSION, 16, 8)
REG32(CXL_DEV_CAP_ARRAY2, 4) /* We're going to pretend it's 64b */
    FIELD(CXL_DEV_CAP_ARRAY2, CAP_COUNT, 0, 16)

/*
 * In the 8.2.8.2, this is listed as a 128b register, but in 8.2.8, it says:
 * > No registers defined in Section 8.2.8 are larger than 64-bits wide so that
 * > is the maximum access size allowed for these registers. If this rule is not
 * > followed, the behavior is undefined
 *
 * Here we've chosen to make it 4 dwords. The spec allows any pow2 multiple
 * access to be used for a register (2 qwords, 8 words, 128 bytes).
 *
 * XXX: This is supposedly fixed for the release version of the spec. If this
 * comment is still here, I've failed.
 */
#define CXL_DEVICE_CAPABILITY_HEADER_REGISTER(n, offset)                            \
    REG32(CXL_DEV_##n##_CAP_HDR0, offset)                 \
        FIELD(CXL_DEV_##n##_CAP_HDR0, CAP_ID, 0, 16)      \
        FIELD(CXL_DEV_##n##_CAP_HDR0, CAP_VERSION, 16, 8) \
    REG32(CXL_DEV_##n##_CAP_HDR1, offset + 4)             \
        FIELD(CXL_DEV_##n##_CAP_HDR1, CAP_OFFSET, 0, 32)  \
    REG32(CXL_DEV_##n##_CAP_HDR2, offset + 8)             \
        FIELD(CXL_DEV_##n##_CAP_HDR2, CAP_LENGTH, 0, 32)

CXL_DEVICE_CAPABILITY_HEADER_REGISTER(DEVICE, CXL_DEVICE_CAP_HDR1_OFFSET)
CXL_DEVICE_CAPABILITY_HEADER_REGISTER(MAILBOX, CXL_DEVICE_CAP_HDR1_OFFSET + \
                                               CXL_DEVICE_CAP_REG_SIZE)

REG32(CXL_DEV_MAILBOX_CAP, 0)
    FIELD(CXL_DEV_MAILBOX_CAP, PAYLOAD_SIZE, 0, 5)
    FIELD(CXL_DEV_MAILBOX_CAP, INT_CAP, 5, 1)
    FIELD(CXL_DEV_MAILBOX_CAP, BG_INT_CAP, 6, 1)
    FIELD(CXL_DEV_MAILBOX_CAP, MSI_N, 7, 4)

REG32(CXL_DEV_MAILBOX_CTRL, 4)
    FIELD(CXL_DEV_MAILBOX_CTRL, DOORBELL, 0, 1)
    FIELD(CXL_DEV_MAILBOX_CTRL, INT_EN, 1, 2)
    FIELD(CXL_DEV_MAILBOX_CTRL, BG_INT_EN, 2, 1)

enum {
    CXL_CMD_EVENTS              = 0x1,
    CXL_CMD_IDENTIFY            = 0x40,
};

REG32(CXL_DEV_MAILBOX_CMD, 8)
    FIELD(CXL_DEV_MAILBOX_CMD, OP, 0, 16)
    FIELD(CXL_DEV_MAILBOX_CMD, LENGTH, 16, 20)

/* 8.2.8.4.5.1 Command Return Codes */
enum {
    RET_SUCCESS                 = 0x0,
    RET_BG_STARTED              = 0x1, /* Background Command Started */
    RET_EINVAL                  = 0x2, /* Invalid Input */
    RET_ENOTSUP                 = 0x3, /* Unsupported */
    RET_ENODEV                  = 0x4, /* Internal Error */
    RET_ERESTART                = 0x5, /* Retry Required */
    RET_EBUSY                   = 0x6, /* Busy */
    RET_MEDIA_DISABLED          = 0x7, /* Media Disabled */
    RET_FW_EBUSY                = 0x8, /* FW Transfer in Progress */
    RET_FW_OOO                  = 0x9, /* FW Transfer Out of Order */
    RET_FW_AUTH                 = 0xa, /* FW Authentication Failed */
    RET_FW_EBADSLT              = 0xb, /* Invalid Slot */
    RET_FW_ROLLBACK             = 0xc, /* Activation Failed, FW Rolled Back */
    RET_FW_REBOOT               = 0xd, /* Activation Failed, Cold Reset Required */
    RET_ENOENT                  = 0xe, /* Invalid Handle */
    RET_EFAULT                  = 0xf, /* Invalid Physical Address */
    RET_POISON_E2BIG            = 0x10, /* Inject Poison Limit Reached */
    RET_EIO                     = 0x11, /* Permanent Media Failure */
    RET_ECANCELED               = 0x12, /* Aborted */
    RET_EACCESS                 = 0x13, /* Invalid Security State */
    RET_EPERM                   = 0x14, /* Incorrect Passphrase */
    RET_EPROTONOSUPPORT         = 0x15, /* Unsupported Mailbox */
    RET_EMSGSIZE                = 0x16, /* Invalid Payload Length */
    RET_MAX                     = 0x17
};

/* XXX: actually a 64b register */
REG32(CXL_DEV_MAILBOX_STS, 0x10)
    FIELD(CXL_DEV_MAILBOX_STS, BG_OP, 0, 1)
    FIELD(CXL_DEV_MAILBOX_STS, ERRNO, 32, 16)
    FIELD(CXL_DEV_MAILBOX_STS, VENDOR_ERRNO, 48, 16)

/* XXX: actually a 64b register */
REG32(CXL_DEV_BG_CMD_STS, 0x18)
    FIELD(CXL_DEV_BG_CMD_STS, BG, 0, 16)
    FIELD(CXL_DEV_BG_CMD_STS, DONE, 16, 7)
    FIELD(CXL_DEV_BG_CMD_STS, ERRNO, 32, 16)
    FIELD(CXL_DEV_BG_CMD_STS, VENDOR_ERRNO, 48, 16)

REG32(CXL_DEV_CMD_PAYLOAD, 0x20)

#endif
